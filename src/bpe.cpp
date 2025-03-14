#include "bpe.h"

#include <cassert>
#include <algorithm>
#include <queue>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <string>
#include <filesystem>
#include <unordered_set>
#include <thread>
#include <optional>
#include <numeric>


namespace bpe {


bool is_space(char c)
{
	return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

bool is_punctuation(char c)
{
	static const std::unordered_set<char> punctuations_set{ ',', '.', '?', '-', '"', ':', ';', '(', ')', '[', ']', '<', '>', '{', '}', '%', '\'', '!', '/', '#', '$', '%', '^', '&', '*', '~', '|', '+', '=' , '\'', '_', '~'  };
	return punctuations_set.find(c) != punctuations_set.end();
}

PrefixBodySuffix split_prefix_body_suffix(std::string_view word)
{
	size_t begin = 0;
	while (begin < word.size() && is_space(word[begin])) {
		begin++;
	}
	if (begin == word.size()) {
		return {"", word, ""};
	}

	size_t body_start = begin;
	while (body_start < word.size() && is_punctuation(word[body_start])) {
		body_start++;
	}

	if (body_start == word.size()) {
		//return PrefixBodySuffix("", word, "");
		return {"", word, ""};
	}

	size_t end = word.size();
	while (end > body_start && is_space(word[end - 1])) {
		end--;
	}

	if (end == body_start) {
		return {"", word, ""};
	}

	size_t body_end = end;
	while (body_end > body_start && is_punctuation(word[body_end - 1])) {
		body_end--;
	}

	if (body_end == body_start) {
		return {"", word, ""};
	}

	const std::string_view prefix = (body_start > begin) ? word.substr(0, body_start) : "";
	const std::string_view suffix = (body_end < end) ? word.substr(body_end, word.size()) : "";
	const std::string_view body = word.substr(prefix.size(), word.size() - prefix.size() - suffix.size());
	return {prefix, body, suffix};
}

std::vector<std::string_view> split_by_words(std::string_view text)
{
	// Find all space intervals.
	std::vector<std::pair<size_t, size_t>> spaces;
	{
		std::optional<size_t> begin;
		for (size_t i = 0; i < text.size(); i++) {
			if (is_space(text[i])) {
				if (!begin) {
					begin = i;
				}
				continue;
			}

			if (begin) {
				spaces.emplace_back(*begin, i);
				begin = std::nullopt;
			}
		}
	}

	// Find split points by gaps.
	std::vector<size_t> split_points;
	split_points.push_back(0); // Begin of the text.
	for (const auto& space : spaces) {
		const size_t space_length = space.second - space.first;
		for (size_t i = 0; i < space_length; i++) {
			split_points.push_back(space.first + i);
		}
	}
	split_points.push_back(text.size()); // End of the text;

	// Split by words.
	std::vector<std::string_view> words;
	words.reserve(split_points.size());
	for (size_t i = 0; i < split_points.size() - 1; i++) {
		const size_t begin = split_points[i];
		const size_t end = split_points[i + 1];
		const std::string_view word{ text.data() + begin, end - begin };

		const PrefixBodySuffix prefix_body_suffix = split_prefix_body_suffix(word);

		const auto prefix = std::get<0>(prefix_body_suffix);
		const auto body = std::get<1>(prefix_body_suffix);
		const auto suffix = std::get<2>(prefix_body_suffix);

		if (!prefix.empty()) {
			words.push_back(prefix);
		}
		if (!body.empty()) {
			words.push_back(body);
		}
		if (!suffix.empty()) {
			words.push_back(suffix);
		}
	}
	return words;
}


// Priority queue that helps find next best candidate for merging.
class TokenizerTrainer::Queue {
public:
	Queue(std::vector<VocabEntry>& _vocab, size_t size);

	// Pop best pair for merging.
	std::optional<Pair> pop();
	// Merge pair of the two consecutive tokens into a new one.
	void merge(const Pair& pair, u32 new_id);

private:
	// Merge candidate - pair of neighboring indices in the corpus.
	struct MergeCandidate {
		// Pair of neighboring indices.
		Pair pair;
		// Pair occurrence in the vocab as it was initially added to the queue.
		u64 queue_count;
		// Real occurrence in the vocab at the current processing moment.
		u64 real_count;
		// Indices in vocab where this pair is located.
		std::unordered_set<size_t> where;

		MergeCandidate(const Pair& _pair, u64 count, size_t start_index) :
			pair(_pair), queue_count(count), real_count(count)
		{
			where.emplace(start_index);
		}

		MergeCandidate(MergeCandidate&& other) noexcept = default;
	};
	// Vocabulary.
	std::vector<VocabEntry>& vocab;
	// Merge candidates storage.
	std::vector<MergeCandidate> candidates;
	// Find merge candidates by indices.
	std::unordered_map<Pair, size_t, PairHash> candidates_index;

	// Comparator for the queue.
	struct CompareByQueueCount {
		explicit CompareByQueueCount(std::vector<MergeCandidate>& _candidates) :
			candidates(_candidates) {}

		bool operator()(u64 left, u64 right) const
			{ return candidates[left].queue_count < candidates[right].queue_count; }
	private:
		std::vector<MergeCandidate>& candidates;
	};

	// Comparison predicate for the queue.
	CompareByQueueCount predicate;
	// Queue, that stores merge candidates ordered by queue_count.
	std::priority_queue<size_t, std::vector<size_t>, CompareByQueueCount> queue;

	std::optional<size_t> pop_non_zero();
	void update_candidate(const Pair& pair, i64 count_delta, size_t where_index);
	void update_candidate_real_count(const Pair& pair, i64 count_delta);
};

TokenizerTrainer::Queue::Queue(std::vector<VocabEntry>& _vocab, size_t size) :
	vocab(_vocab),
	predicate(candidates),
	queue(predicate)
{
	assert(size > 0);

	candidates.reserve(size);
	candidates_index.reserve(size);

	std::sort(vocab.begin(), vocab.end(),
		[](const VocabEntry& first, const VocabEntry& second) {
			return first.count > second.count;
		}
	);

	for (size_t vocab_index = 0; vocab_index < vocab.size(); vocab_index++) {
		const VocabEntry& item = vocab[vocab_index];

		for (size_t i = 1; i < item.ids.size(); i++) {
			const Pair pair{ item.ids[i - 1], item.ids[i] };
			update_candidate(pair, to<i64>(item.count), vocab_index);
		}
	}

	for (size_t i = 0; i < candidates.size(); i++) {
		queue.push(i);
	}
}

// Pop an element from the queue with a non-zero real_count.
// If there is no such element, return -1.
std::optional<size_t> TokenizerTrainer::Queue::pop_non_zero()
{
	while (true) {
		if (queue.empty()) {
			return std::nullopt;
		}

		const auto index = queue.top();
		queue.pop();
		const auto& candidate = candidates[index];
		if (candidate.real_count != 0) {
			return index;
		}
	}
}

std::optional<Pair> TokenizerTrainer::Queue::pop()
{
	while (true) {
		const std::optional<size_t> index = pop_non_zero();
		if (!index.has_value()) {
			return std::nullopt;
		}

		auto& candidate = candidates[index.value()];
		if (candidate.real_count == candidate.queue_count) {
			return candidate.pair;
		}

		candidate.queue_count = candidate.real_count;
		queue.push(index.value());
	}
}

void TokenizerTrainer::Queue::merge(const Pair& pair, u32 new_id)
{
	const size_t index = candidates_index[pair];

	std::unordered_set<Pair, PairHash> new_pairs;
	for (size_t vocab_index : candidates[index].where) {
		const auto& entry = vocab[vocab_index];
		const auto& ids = entry.ids;
		const i64 count = to<i64>(entry.count);

		std::vector<u32> new_ids;
		new_ids.reserve(ids.size());

		size_t i = 0;
		while (i < ids.size()) {
			if (i + 1 < ids.size() && ids[i] == pair.first && ids[i + 1] == pair.second) {
				if (i > 0) {
					const Pair left_pair{ ids[i - 1], ids[i] };
					update_candidate_real_count(left_pair, -count);

					const Pair new_left_pair{ new_ids.back(), new_id };
					update_candidate(new_left_pair, count, vocab_index);
					new_pairs.emplace(new_left_pair);
				}
				if (i + 2 < ids.size()) {
					const Pair right_pair{ ids[i + 1], ids[i + 2] };
					update_candidate_real_count(right_pair, -count);

					const Pair new_right_pair{ new_id, ids[i + 2] };
					update_candidate(new_right_pair, count, vocab_index);
					new_pairs.emplace(new_right_pair);
				}
				new_ids.push_back(new_id);
				i += 2;
			} else {
				new_ids.push_back(ids[i]);
				i += 1;
			}
		}
		vocab[vocab_index].ids = new_ids;
	}
	candidates[index].where.clear();
	candidates[index].real_count = 0;
	candidates[index].queue_count = 0;

	for (const auto& new_pair : new_pairs) {
		queue.push(candidates_index[new_pair]);
	}
}

// After merge candidate count, real_count and where should be updated.
void TokenizerTrainer::Queue::update_candidate(const Pair& pair, i64 count_delta, size_t where_index)
{
	if (!candidates_index.contains(pair)) {
		const u32 index = static_cast<u32>(candidates.size());
		candidates_index.emplace(pair, index);
		candidates.emplace_back(pair, count_delta, where_index);
	} else {
		const size_t index = candidates_index[pair];
		MergeCandidate& candidate = candidates[index];
		candidate.queue_count += static_cast<size_t>(count_delta);
		candidate.real_count += static_cast<size_t>(count_delta);
		candidate.where.emplace(where_index);
	}
}

void TokenizerTrainer::Queue::update_candidate_real_count(const Pair& pair, i64 count_delta)
{
	candidates[candidates_index[pair]].real_count += static_cast<size_t>(count_delta);
}

void TokenizerTrainer::init_id_to_seq()
{
	id_to_seq.reserve(byte_count);
	for (size_t i = 0; i < byte_count; i++) {
		id_to_seq.push_back(std::string{ static_cast<char>(i) });
	}
}

void TokenizerTrainer::create_vocab_from_word_vocab()
{
	vocab.reserve(word_vocab.size());
	for (const auto& item : word_vocab) {
		if (item.second < config.min_count) {
			continue;
		}
		VocabEntry entry;
		entry.ids.reserve(item.first.size());
		for (auto id : item.first) {
			entry.ids.push_back(static_cast<u8>(id));
		}
		entry.count = item.second;
		entry.text = item.first;
		vocab.push_back(entry);
	}
}

static void build_vocabulary_single_thread(
	const std::string& path, size_t begin, size_t end, 
	std::unordered_map<std::string, u64>& word_vocab)
{
	std::ifstream file{ path, std::ios_base::in };
	file.seekg(std::streampos{ to<std::streamoff>(begin) });

	static constexpr size_t vocabulary_init_size = 1000000;

	std::string line;
	word_vocab.reserve(vocabulary_init_size);
	while (std::getline(file, line)) {
		std::vector<std::string_view> words = split_by_words(line);
		for (const auto& word : words) {
			if (word.empty()) {
				continue;
			}
			std::string word_str{ word };
			if (word_vocab.find(word_str) == word_vocab.end()) {
				word_vocab[word_str] = 1;
			} else {
				word_vocab[word_str]++;
			}
		}
		if (static_cast<size_t>(file.tellg()) >= end) {
			break;
		}
	}
}

static void build_vocabulary_multiple_threads(
	const std::string& path, size_t file_size, u32 max_worker,
	std::unordered_map<std::string, u64>& word_vocab)
{
	const size_t chunk_size = file_size / max_worker;
	assert(chunk_size >= 1);

	// Split work.
	std::vector<std::pair<size_t, size_t>> ranges;
	ranges.reserve(max_worker);
	for (size_t i = 0; i < max_worker; i++) {
		ranges.emplace_back(i * chunk_size, std::min<size_t>(file_size, (i + 1) * chunk_size));
	}

	// Start all threads.
	std::vector<std::unordered_map<std::string, u64>> word_vocabs(ranges.size());
	std::vector<std::thread> threads;
	for (size_t i = 0; i < ranges.size(); i++) {
		const auto& range = ranges[i];
		std::thread thread{ build_vocabulary_single_thread, path, range.first, range.second, std::ref(word_vocabs[i])};
		threads.push_back(std::move(thread));
	}

	// Wait for all threads to terminate.
	for (auto& thread : threads) {
		thread.join();
	}

	// Merge vocabularies from threads into one vocabulary.
	word_vocab.reserve(2 * word_vocabs.front().size());
	for (const auto& thread_vocab : word_vocabs) {
		for (const auto& [word, count] : thread_vocab) {
			if (!word_vocab.contains(word)) {
				word_vocab.emplace(word, count);
			} else {
				word_vocab[word] += count;
			}
		}
	}
}

std::vector<u8> TokenizerTrainer::save() const
{
	std::vector<u8> buffer;

	ShortStringsMappedArray::write_to_buffer(id_to_seq, buffer);
	MergeTable::write_to_buffer(merge_table, buffer);
	Cache::write_to_buffer(cache, buffer);

	return buffer;
}

void TokenizerTrainer::build_bpe()
{
	assert(id_to_seq.empty());
	assert(merge_table.empty());

	init_id_to_seq();
	create_vocab_from_word_vocab();
	train_bpe();
	build_cache();
}

void TokenizerTrainer::train_bpe()
{
	assert(config.size >= byte_count);

	const size_t num_merges = config.size - byte_count;

	Queue queue{ vocab, config.size };
	for (size_t i = 0; i < num_merges; i++) {
		const std::optional<Pair> pair = queue.pop();
		if (!pair.has_value()) {
			break;
		}

		const u32 new_id = to<u32>(id_to_seq.size());
		merge_table[*pair] = new_id;
		const std::string new_id_ids = id_to_seq[pair->first] + id_to_seq[pair->second];
		id_to_seq.push_back(new_id_ids);

		queue.merge(*pair, new_id);
	}
}

// Build bpe cache.
void TokenizerTrainer::build_cache()
{
	if (config.cache_size == 0) {
		return;
	}

	std::cout << "Build cache\n";
	const auto cache_size = std::min<size_t>(config.cache_size, vocab.size());
	for (size_t i = 0; i < cache_size; i++) {
		const auto& entry = vocab[i];
		cache.emplace(entry.text, entry.ids);
	}
}

void TokenizerTrainer::build_vocabulary_on_text(const std::string& text)
{
	std::vector<std::string_view> words = split_by_words(text);
	for (const auto& word : words) {
		const std::string word_str{ word };
		if (!word_vocab.contains(word_str)) {
			word_vocab[word_str] = 1;
		} else {
			word_vocab[word_str]++;
		}
	}
}

void TokenizerTrainer::build_vocabulary(const std::string& path, size_t symbols_count)
{
	size_t file_size = std::filesystem::file_size(path);
	if (symbols_count > 0) {
		file_size = std::min<size_t>(file_size, symbols_count);
	}

	constexpr size_t single_thread_file_size = 16384;

	if (config.max_worker == 1 || file_size <= single_thread_file_size) {
		build_vocabulary_single_thread(path, 0, file_size, word_vocab);
	} else {
		build_vocabulary_multiple_threads(path, file_size, config.max_worker, word_vocab);
	}
}

void TokenizerTrainer::train_on_corpus(const std::string& path, size_t symbols_count)
{
	build_vocabulary(path, symbols_count);
}

void TokenizerTrainer::train_on_text(const std::string& text)
{
	build_vocabulary_on_text(text);
}

Tokenizer::Tokenizer(const std::filesystem::path& path)
{
	load(path);
}

void Tokenizer::load(const std::filesystem::path& path)
{
	memory = load_file_to_buffer(path);
	attach(memory.data());
}

void Tokenizer::attach(const u8* data)
{
	size_t offset = 0;
	offset += id_to_seq.attach(data + offset);
	offset += merge_table.attach(data + offset);
	cache.attach(data + offset);
}

std::vector<u32> Tokenizer::encode(std::string_view text) const
{
	std::vector<u32> ids;
	ids.reserve(text.size());

	const std::vector<std::string_view> words = split_by_words(text);
	for (const auto& word : words) {
		if (cache.contains(word)) {
			const std::vector<u32> word_ids = cache.get(word);
			ids.insert(ids.end(), word_ids.begin(), word_ids.end());
		} else {
			const std::vector<u32> word_ids = encode_word(word);
			ids.insert(ids.end(), word_ids.begin(), word_ids.end());
		}
	}
	return ids;
}

std::vector<u32> Tokenizer::encode_word(std::string_view text) const
{
	std::vector<u32> ids;
	ids.reserve(text.size());
	for (char c: text) {
		ids.push_back(static_cast<u32>(c));
	}

	while (ids.size() >= 2) {

		using MinIdWithIndex = std::pair<u32, size_t>;
		std::optional<MinIdWithIndex> min_id;
		for (size_t i = 1; i < ids.size(); i++) {
			const std::optional<u32> id = get_merge_id(ids[i - 1], ids[i]);
			if (id) {
				if (!min_id) {
					min_id = MinIdWithIndex{*id, i - 1};
				} else {
					if (id < min_id->first) {
						min_id->first = *id;
						min_id->second = i - 1;
					}
				}
			}
		}

		if (!min_id) {
			break;
		}

		const auto& [new_id, index] = *min_id;

		ids[index] = new_id;
		for (size_t i = index + 1; i < ids.size() - 1; i++) {
			ids[i] = ids[i + 1];
		}
		ids.resize(ids.size() - 1);
	}

	return ids;
}

std::string Tokenizer::decode(const std::vector<u32>& ids) const
{
	return std::accumulate(ids.begin(), ids.end(), std::string{}, [this](const std::string& acc, u32 id) {
		return acc + std::string(id_to_seq[id]);
	});
}

std::string_view Tokenizer::decode_token(u32 id) const
{
	assert(id < id_to_seq.size());
	return id_to_seq[id];
}

std::optional<u32> Tokenizer::get_merge_id(u32 first, u32 second) const
{
	const Pair merge_pair{ first, second };
	if (!merge_table.contains(merge_pair)) {
		return std::nullopt;
	}
	return merge_table.get(merge_pair);
}

} // namespace bpe