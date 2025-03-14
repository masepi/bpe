# BPE Tokenizer

A fast and lightweight Byte Pair Encoding (BPE) tokenizer offering efficient training, inference, and dictionary loading. 
The tokenizer achieves quick loading by memory-mapping all data structures on disk, significantly reducing startup time.

## Features

- **Fast Training**: Quickly train BPE merges on your custom dataset.
- **Efficient Inference**: High-performance tokenization and detokenization.
- **Memory-Mapped Data**: All internal data structures are mapped to disk, enabling lightning-fast loading.
- **Easy Integration**: Simple API for training, loading, and tokenizing.


## Usage

```c++

using namespace bpe;

TokenizerTrainer::Config config;
config.size = 16384;
config.min_count = 1;
config.cache_size = 10;
config.max_worker = 1;

const std::filesystem::path path = "test_corpus.txt";

TokenizerTrainer trainer{ config };
trainer.train_on_corpus(path.string(), 0);
trainer.build_bpe();

const ByteBuffer tokenizer_buffer = trainer.save();
Tokenizer bpe;
bpe.attach(tokenizer_buffer.data());

```