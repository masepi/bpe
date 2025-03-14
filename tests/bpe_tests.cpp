
#include "bpe.h"

// Potential comparison of a constant with another constant in EXPECT checks
#include <gtest/gtest.h>

using namespace bpe;

TEST(bpe, split_by_words)
{
	EXPECT_EQ(split_by_words("hello world"), std::vector<std::string_view>({"hello", " world"}));
	EXPECT_EQ(split_by_words("hello  world"), std::vector<std::string_view>({"hello", " ", " world"}));
	EXPECT_EQ(split_by_words("hello, world"), std::vector<std::string_view>({"hello", ",", " world"}));
	EXPECT_EQ(split_by_words("Hello, world!"), std::vector<std::string_view>({"Hello", ",", " world", "!"}));
}

TEST(bpe, split_prefix_body_suffix)
{
	auto split = [](std::string_view word, std::string_view p, std::string_view b, std::string_view s) -> bool {
		const auto pbs = split_prefix_body_suffix(word);

		bool is_correct = true;
		if (p != std::get<0>(pbs)) {
			is_correct = false;
		}
		if (b != std::get<1>(pbs)) {
			is_correct = false;
		}
		if (s != std::get<2>(pbs)) {
			is_correct = false;
		}
		return is_correct;
	};

	ASSERT_TRUE(split("", "", "", ""));
	ASSERT_TRUE(split("Hello", "", "Hello", ""));
	ASSERT_TRUE(split(" Hello", "", " Hello", ""));
	ASSERT_TRUE(split("  Hello", "", "  Hello", ""));
	ASSERT_TRUE(split("  Hello ", "", "  Hello ", ""));
	ASSERT_TRUE(split("  Hello  ", "", "  Hello  ", ""));
	ASSERT_TRUE(split("(Hello", "(", "Hello", ""));
	ASSERT_TRUE(split("(Hello,!", "(", "Hello", ",!"));
	ASSERT_TRUE(split("Hello,", "", "Hello", ","));
	ASSERT_TRUE(split(" (Hello", " (", "Hello", ""));
	ASSERT_TRUE(split("  (Hello", "  (", "Hello", ""));
	ASSERT_TRUE(split("  (Hello)", "  (", "Hello", ")"));
	ASSERT_TRUE(split("  (Hello) ", "  (", "Hello", ") "));
	ASSERT_TRUE(split(",,,,", "", ",,,,", ""));
}

// Test fixture for setting up and tearing down the tests
class BpeCorpusTest : public ::testing::Test {
protected:

	void SetUp() override
	{
		TokenizerTrainer::Config config;
		config.size = 16384;
		config.min_count = 1;
		config.cache_size = 10;
		config.max_worker = 1;

		const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "test_corpus.txt";

		TokenizerTrainer trainer{ config };
		trainer.train_on_corpus(path.string(), 0);
		trainer.build_bpe();

		tokenizer_buffer = trainer.save();
		bpe.attach(tokenizer_buffer.data());
	}
	Tokenizer bpe;

private:
	ByteBuffer tokenizer_buffer;
};

TEST(BpeTest, tokenize)
{
	TokenizerTrainer::Config config;
	config.size = 256 + 10;
	config.min_count = 1;
	config.cache_size = 10;
	config.max_worker = 1;

	TokenizerTrainer trainer{ config };
	trainer.train_on_text("Hello, world!");
	trainer.build_bpe();

	const ByteBuffer buffer = trainer.save();
	Tokenizer tokenizer;
	tokenizer.attach(buffer.data());

	const auto ids = tokenizer.encode("Hello, world!");
	
	std::vector<std::string_view> tokens;
	tokens.reserve(ids.size());
	for (const auto& id : ids) {
		tokens.push_back(tokenizer.decode_token(id));
	}
	ASSERT_EQ(tokens[0], "Hello");
}

TEST_F(BpeCorpusTest, encode_decode)
{
	auto encode_decode = [this](std::string_view text) -> bool {
		const auto ids = bpe.encode(text);
		const std::string decoded_text = bpe.decode(ids);
		return decoded_text == text;
	};

	ASSERT_TRUE(encode_decode(""));
	ASSERT_TRUE(encode_decode(" "));
	ASSERT_TRUE(encode_decode("  "));
	ASSERT_TRUE(encode_decode("Hello, world!"));
	ASSERT_TRUE(encode_decode(" Hello, world!"));
	ASSERT_TRUE(encode_decode("  Hello, world!"));
	ASSERT_TRUE(encode_decode("   Hello, world!"));
	ASSERT_TRUE(encode_decode("Hello, world! "));
	ASSERT_TRUE(encode_decode("Hello, world!  "));
	ASSERT_TRUE(encode_decode("Hello, world!   "));
}