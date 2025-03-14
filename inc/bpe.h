#pragma once

#include <utility>
#include <vector>
#include <unordered_map>
#include <string>
#include <tuple>
#include <optional>

#include "mapped_storages.h"

namespace bpe {

using PrefixBodySuffix = std::tuple<std::string_view, std::string_view, std::string_view>;

// Split word str to prefix, body, suffix with respect to the start and finish spaces.
PrefixBodySuffix split_prefix_body_suffix(std::string_view word);

// Split text by words. Initial spaces will be glued to the right word.
std::vector<std::string_view> split_by_words(std::string_view text);

// Pair of two consecutive indices.
using Pair = std::pair<u32, u32>;

// Hash for the pair.
struct PairHash {
	size_t operator() (const Pair& pair) const { return pair.first | (static_cast<size_t>(pair.second) << 32); }
};

using MergeTable = MappedMap<Pair, u32, DefaultMapConfig<Pair, u32, PairHash>>;
using Cache = MappedMap<std::string_view, std::vector<u32>, DefaultMapConfig<std::string_view, std::vector<u32>>>;

// Bpe tokenizer trainer.
class TokenizerTrainer {
public:
	struct Config {
		// Bpe vocabulary size. >= 256
		size_t size;
		// Minimum number of times a word must appear in a corpus to be processed by BPE algorithm.
		size_t min_count;
		// Maximum number of processing threads.
		u32 max_worker;
		// Bpe cache size. Cache is the map of the cache_size most frequent words into the precalculated ids.
		size_t cache_size;

		Config() : size(256), min_count(1), max_worker(1), cache_size(0) {}
	};

	explicit TokenizerTrainer(const Config& _config) : config(_config) 
	{ 
		assert(config.size >= byte_count); 
		assert(config.max_worker >= 1);
	}

	// Train bpe methods. These methods can be called multiple times.
	// Train bpe tokenizer on a corpus.
	// symbols_count - Number of bytes from the start to train tokenizer, 0 - all file.
	void train_on_corpus(const std::string& path, size_t symbols_count);
	// Train bpe tokenizer on an arbitrary text.
	void train_on_text(const std::string& text);

	// Build bpe. This method should be called after train_on_* methods.
	void build_bpe();

	const std::unordered_map<Pair, u32, PairHash>& get_merge_table() const { return merge_table; }
	const std::vector<std::string>& get_id_to_seq() const { return id_to_seq; }

	// Save tokenizer to a byte array.
	std::vector<u8> save() const;

private:
	const Config config;
	// Merge table.
	std::unordered_map<Pair, u32, PairHash> merge_table;
	// Tokens sequences
	std::vector<std::string> id_to_seq;
	// Precomputed cache for most frequent words.
	std::unordered_map<std::string, std::vector<u32>> cache;

	static constexpr size_t byte_count = 256;

	class Queue;

	// Single vocabulary entry.
	struct VocabEntry {
		// Tokens ids.
		std::vector<u32> ids;
		// Word text.
		std::string text;
		// How many times this word appears in the corpus.
		u64 count;

		VocabEntry() : count(0) {}
	};
	using Vocab = std::vector<VocabEntry>;

	// Vocabulary.
	std::unordered_map<std::string, u64> word_vocab;
	Vocab vocab;

	void train_bpe();
	void build_cache();
	void build_vocabulary_on_text(const std::string& text);
	void init_id_to_seq();
	void create_vocab_from_word_vocab();
	void build_vocabulary(const std::string& path, size_t symbols_count);
};


// Byte pair encoding on UTF-8 text.
class Tokenizer {
public:
	explicit Tokenizer(const std::filesystem::path& path);
	Tokenizer() = default;

	// Load tokenizer from memory.
	void load(const std::filesystem::path& path);
	// Attach external buffer. Do not copy data!
	void attach(const u8* data);

	// Encode text.
	std::vector<u32> encode(std::string_view text) const;
	// Decode sequence of token ids.
	std::string decode(const std::vector<u32>& ids) const;
	// Decode the single token.
	std::string_view decode_token(u32 id) const;
	
private:
	// Memory holding all tokenizer data.
	std::vector<u8> memory;
	// Tokens sequences.
	ShortStringsMappedArray id_to_seq;
	// Merge table.
	MergeTable merge_table;
	// Cache for most frequent words.
	Cache cache;

	std::vector<u32> encode_word(std::string_view text) const;
	std::optional<u32> get_merge_id(u32 first, u32 second) const;
};

} // namespace bpe
