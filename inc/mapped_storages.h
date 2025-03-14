#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <filesystem>
#include <cassert>
#include <array>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <climits>

#include "to.h"


namespace bpe {

using ByteBuffer = std::vector<u8>;

// Load entire file to a buffer.
ByteBuffer load_file_to_buffer(const std::filesystem::path& path);

// Class that read typed values from the bytes buffer.
class BufferReader {
public:
	explicit BufferReader(const u8* _data) : data(_data) {}

	u8 read_u8()
	{ 
		const u8* prev = data;
		data++;
		return *prev; 
	}

	u16 read_u16() 
	{
		const u8* prev = data;
		data += sizeof(u16);
		return *reinterpret_cast<const u16*>(prev);
	}

	u32 read_u32() 
	{ 
		const u8* prev = data;
		data += sizeof(u32);
		return *reinterpret_cast<const u32*>(prev);
	}

	std::string_view read_string_view()
	{
		const size_t size = read_u8();
		const u8* prev = data;
		data += size;
		return std::string_view(reinterpret_cast<const char*>(prev), size);
	}

	void skip_string_view() 
	{ 
		const size_t size = read_u8();
		data += size; 
	}

	template<typename T>
	T read()
	{
		const u8* prev = data;
		data += sizeof(T);
		return *reinterpret_cast<const T*>(prev);
	}

	template<typename T>
	void skip() { data += sizeof(T); }

	void skip_count(size_t count) { data += count; }

	const u8* ptr() const { return data; }

private:
	const u8* data;
};


class BufferWriter {
public:
	explicit BufferWriter(u8* _data) : data(_data) {}

	void write_u8(u8 value)
	{
		*data = value;
		data++;
	}

	void write_u16(u16 value)
	{
		::memcpy(data, reinterpret_cast<const u8*>(&value), sizeof(u16));
		data += sizeof(u16);
	}

	void write_u32(u32 value)
	{
		::memcpy(data, reinterpret_cast<const u8*>(&value), sizeof(u32));
		data += sizeof(u32);
	}

	void write_string_view(std::string_view value)
	{
		const size_t size = value.size();
		assert(size <= 0xFF);
		write_u8(static_cast<u8>(size));

		::memcpy(data, value.data(), size);
		data += size;
	}

	template<typename T>
	void write(T value)
	{
		::memcpy(data, reinterpret_cast<const u8*>(&value), sizeof(T));
		data += sizeof(T);
	}

	u8* ptr() const { return data; }

private:
	u8* data;
};


template<typename T>
class DataSerializer {
public:
	void write(T value, BufferWriter& writer) { writer.write(value); }
	T read(BufferReader& reader) { return reader.read<T>(); }
	void skip(BufferReader& reader) { reader.skip<T>(); }
	size_t size(T) const { return sizeof(T); }
};

template<>
class DataSerializer<std::string_view> {
public:
	void write(std::string_view value, BufferWriter& writer)
		{ writer.write_string_view(value); }

	std::string_view read(BufferReader& reader)
		{ return reader.read_string_view(); }
	void skip(BufferReader& reader) { reader.skip_string_view(); }

	size_t size(std::string_view value) const { return 1 + value.size(); }
};

template<typename T>
class DataSerializer<std::vector<T>> {
public:
	void write(const std::vector<T>& value, BufferWriter& writer)
	{
		writer.write_u32(static_cast<u32>(value.size()));
		for (T x : value) {
			writer.write<T>(x);
		}
	}

	std::vector<T> read(BufferReader& reader)
	{
		const size_t size = reader.read_u32();
		std::vector<T> result;
		result.reserve(size);
		for (size_t i = 0; i < size; i++) {
			result.push_back(reader.read<T>());
		}
		return result;
	}
	void skip(BufferReader& reader)
	{ 
		const size_t size = reader.read_u32();
		reader.skip_count(size * sizeof(T));
	}

	size_t size(const std::vector<T>& value) const { return 4 + value.size() * sizeof(T); }
};


// Mapped storage for short (string length <= 256) strings.
class ShortStringsMappedArray {
public:
	explicit ShortStringsMappedArray(const u8* data);
	ShortStringsMappedArray();

	// Attach the external buffer and return buffer size.
	size_t attach(const u8* data);

	// Create ShortStringsMappedArray from the vector of strings, write it to the buffer and return buffer size.
	static size_t write_to_buffer(const std::vector<std::string>& data, std::vector<u8>& buffer);

	// Collection size.
	size_t size() const { return element_count; }
	// Get string by index.
	std::string_view operator[](size_t index) const;

private:
	size_t buffer_size;
	u32 element_count;
	const u8* offsets;
	const u8* strings;

/*
                             	    Layout in the file.
	╔══════════════════╦══════════════════╦══════════════════╦═══════════════════════════════════════╗
	║ Offset (bytes)   ║   Size (bytes)   ║ Field            ║ Description                           ║
	╠══════════════════╬══════════════════╬══════════════════╬═══════════════════════════════════════╣
	║ 0                ║        4         ║ buffer_size      ║ Total buffer size (u32 little-endian) ║
	╠══════════════════╬══════════════════╬══════════════════╬═══════════════════════════════════════╣
	║ 4                ║        4         ║ element_count    ║ Number of elements (u32 little-endian)║
	╠══════════════════╬══════════════════╬══════════════════╬═══════════════════════════════════════╣
	║ 8                ║     N × 4        ║ offsets          ║ Array of string offsets (u32 LE)      ║
	║                  ║ (N=element_count)║                  ║ Relative to strings section start     ║
	╠══════════════════╬══════════════════╬══════════════════╬═══════════════════════════════════════╣
	║ 8 + N×4          ║    Variable      ║ strings          ║ Packed strings: [1-byte length][data] ║
	║                  ║                  ║                  ║ Max length: 255, no null-terminator   ║
	╚══════════════════╩══════════════════╩══════════════════╩═══════════════════════════════════════╝
*/

};


// Config trait for map.
template<
	typename _Key, typename _Value,
	typename _KeyHash=std::hash<_Key>, 
	typename _KeyEq=std::equal_to<_Key>,
	typename _KeySerializer=DataSerializer<_Key>,
	typename _ValueSerializer=DataSerializer<_Value>
>
struct DefaultMapConfig {
	using Key = _Key;
	using Value = _Value;
	using KeyHash = _KeyHash;
	using KeyEq = _KeyEq;
	using KeySerializer = _KeySerializer;
	using ValueSerializer = _ValueSerializer;
};

// Mapped storage for arbitrary key-value pairs.
template<typename Key, typename Value, typename Config=DefaultMapConfig<Key, Value>>
class MappedMap {
public:

	static_assert(std::is_same_v<Key, typename Config::Key>);
	static_assert(std::is_same_v<Value, typename Config::Value>);

	using key_type=Key;
	using mapped_type=Value;

	explicit MappedMap(const u8* data) : 
		buffer_size(0),
		number_of_elements(0), 
		hash_table_size(0),
		end_pos(0),
		index(nullptr),
		storage(nullptr)
	{ 
		attach(data); 
	}
	MappedMap() :
		buffer_size(0),
		number_of_elements(0),
		hash_table_size(0),
		end_pos(0),
		index(nullptr),
		storage(nullptr) {}

	// Attach the external buffer and return buffer size.
	size_t attach(const u8* data);

	// Create MappedMap from the map, write it to the buffer and return buffer size.
	template<typename Map>
	static size_t write_to_buffer(const Map& data, std::vector<u8>& buffer);

	// Check if the map contains the key.
	bool contains(const Key& key) const;
	// Get the value by the key.
	Value get(const Key& key) const;
	// Get the value by the key.
	Value operator[](const Key& key) const { return get(key); }
	// Collection size.
	size_t size() const { return number_of_elements; }

	// Simple iteration over the map.
	using Position = u32;
	Position get_begin_position() const { return 0; }
	Position get_end_position() const { return end_pos; }
	Position get_next_position(Position pos) const;
	std::pair<Key, Value> get_key_value(Position pos) const;

private:
	size_t buffer_size;
	u32 number_of_elements;
	u32 hash_table_size;
	u32 end_pos;
	const u8* index;
	const u8* storage;

/*                                        Layout in the file.
╔══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════╗
║ Offset (bytes)  Size (bytes)    Field                 Description                                                    ║
╠══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════╣
║ 0               4               buffer_size           Total size of mapped buffer (header + index + storage)         ║
╟──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╢
║ 4               4               number_of_elements    Total number of key-value pairs in the map                     ║
╟──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╢
║ 8               4               hash_table_size       Size of hash table (number of buckets)                         ║
╟──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╢
║ 12              4               end_pos               Offset to end of valid data in storage                         ║
╠══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════╣
║ 16              8*N             index                 Hash table index (N = hash_table_size):                        ║
║                                [bucket_0]             ┌────────────────────┬────────────────────┐                    ║
║                                                       │   entry_offset     │   entry_end_offset │                    ║
║                                [bucket_1]             └────────────────────┴────────────────────┘                    ║
║                                                       ... (repeated hash_table_size times) ...                       ║
╠══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════╣
║ 16+8*N          Variable        storage               Key-value storage area:                                        ║
║                                [entry_0]              ┌───────────────────────────────┐                              ║
║                                                       │ Serialized Key (variable size)│                              ║
║                                                       ├───────────────────────────────┤                              ║
║                                                       │ Serialized Value (var size)   │                              ║
║                                                       └───────────────────────────────┘                              ║
║                                [entry_1]              ... (repeated number_of_elements times) ...                    ║
╚══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════╝
*/
	static constexpr u32 unknown_offset = std::numeric_limits<u32>::max();

	template<typename Map>
	static size_t choose_hash_table_size(const Map& data);
	static std::vector<size_t> find_prime_numbers(size_t n);
};

template<typename Key, typename Value, typename Config>
size_t MappedMap<Key, Value, Config>::attach(const u8* data)
{
	assert(data != nullptr);

	BufferReader reader{ data };

	buffer_size = reader.read_u32();
	number_of_elements = reader.read_u32();
	hash_table_size = reader.read_u32();
	end_pos = reader.read_u32();
	index = data + 4 * sizeof(u32);
	storage = index + hash_table_size * ( 2 * sizeof(u32));

	return buffer_size;
}

template<typename Key, typename Value, typename Config>
inline bool MappedMap<Key, Value, Config>::contains(const Key& key) const
{
	typename Config::KeyHash hasher;
	typename Config::KeyEq eq;
	typename Config::KeySerializer key_serializer;
	typename Config::ValueSerializer value_serializer;

	const size_t entry_index = hasher(key) % hash_table_size;

	BufferReader index_reader{ index + 2 * sizeof(u32) * entry_index };

	const u32 offset = index_reader.read_u32();
	if (offset == unknown_offset || offset >= end_pos) {
		return false;
	}
	const u32 end_key_offset = index_reader.read_u32();
	assert(end_key_offset <= end_pos);

	BufferReader storage_reader{ storage + offset };
	while (storage_reader.ptr() - storage < end_key_offset) {
		const auto storage_key = key_serializer.read(storage_reader);
		if (eq(key, storage_key)) {
			return true;
		}
		value_serializer.skip(storage_reader);
	}
	return false;
}

template<typename Key, typename Value, typename Config>
inline Value MappedMap<Key, Value, Config>::get(const Key& key) const
{
	typename Config::KeyHash hasher;
	typename Config::KeyEq eq;
	typename Config::KeySerializer key_serializer;
	typename Config::ValueSerializer value_serializer;

	const size_t entry_index = hasher(key) % hash_table_size;

	BufferReader index_reader{ index + 2 * sizeof(u32) * entry_index };

	const u32 offset = index_reader.read_u32();
	assert(offset != unknown_offset && offset < end_pos);
	const u32 end_key_offset = index_reader.read_u32();
	assert(end_key_offset <= end_pos);

	BufferReader storage_reader{ storage + offset };
	while (storage_reader.ptr() - storage < end_key_offset) {
		const auto storage_key = key_serializer.read(storage_reader);
		if (eq(key, storage_key)) {
			return value_serializer.read(storage_reader);
		}
		value_serializer.skip(storage_reader);
	}
	return Value();
}

template<typename Key, typename Value, typename Config>
inline auto MappedMap<Key, Value, Config>::get_next_position(Position pos) const -> Position
{
	BufferReader reader{ storage + pos };

	typename Config::KeySerializer key_serializer;
	typename Config::ValueSerializer value_serializer;

	// skip object.
	key_serializer.skip(reader);
	value_serializer.skip(reader);

	return static_cast<Position>(reader.ptr() - storage);
}

template<typename Key, typename Value, typename Config>
inline std::pair<Key, Value>
MappedMap<Key, Value, Config>::get_key_value(Position pos) const
{
	BufferReader reader{storage + pos};

	typename Config::KeySerializer key_serializer;
	typename Config::ValueSerializer value_serializer;

	const auto key = key_serializer.read(reader);
	const auto value = value_serializer.read(reader);

	return std::pair<Key, Value>(std::move(key), std::move(value));
}

template<typename Key, typename Value, typename Config>
template<typename Map>
inline size_t MappedMap<Key, Value, Config>::write_to_buffer(
	const Map& data, std::vector<u8>& buffer)
{
	const size_t hash_table_size = choose_hash_table_size(data);
	assert(hash_table_size != 0);

	typename Config::KeyHash hasher;
	typename Config::KeySerializer key_serializer;
	typename Config::ValueSerializer value_serializer;

	struct MapData {
		const typename Map::key_type* key;
		const typename Map::mapped_type* value;

		MapData() : key(nullptr), value(nullptr) {}
		MapData(const typename Map::key_type* _key, const typename Map::mapped_type* _value):
			key(_key), value(_value) {}
	};

	std::vector<std::vector<MapData>> index_vector(hash_table_size);
	for (const auto& item : data) {
		const auto& key = item.first;
		const auto& value = item.second;

		const size_t key_hash = hasher(key);
		const size_t index = key_hash % hash_table_size;

		index_vector[index].emplace_back(&key, &value);
	}

	size_t storage_size = 0;
	for (const auto& entry : index_vector) {
		if (entry.empty()) {
			continue;
		}

		for (const auto& map_data : entry) {
			storage_size += key_serializer.size(*map_data.key); // key
			storage_size += value_serializer.size(*map_data.value); // value
		}
	}

	const size_t header_size = 4 * sizeof(u32);
	const size_t index_size = hash_table_size * 2 * sizeof(u32);

	const size_t buffer_size = header_size + index_size + storage_size;
	const size_t prev_pos = buffer.size();
	buffer.resize(buffer.size() + buffer_size);

	u8* base_ptr = buffer.data() + prev_pos;

	BufferWriter header_writer{ base_ptr };
	header_writer.write_u32(static_cast<u32>(buffer_size));
	header_writer.write_u32(static_cast<u32>(data.size()));
	header_writer.write_u32(static_cast<u32>(hash_table_size));
	header_writer.write_u32(0); // stub for the end_pos;

	BufferWriter index_writer{ base_ptr + header_size };

	u8* storage_base_ptr = base_ptr + header_size + index_size;
	BufferWriter storage_writer{ storage_base_ptr };

	for (size_t pos = 0; pos < index_vector.size(); pos++) {
		const auto& entry = index_vector[pos];
		if (entry.empty()) {
			index_writer.write_u32(unknown_offset);
			index_writer.write_u32(unknown_offset);
		} else {
			index_writer.write_u32(static_cast<u32>(storage_writer.ptr() - storage_base_ptr)); // begin

			for (const auto& map_data : entry) {
				key_serializer.write(*map_data.key, storage_writer);
				value_serializer.write(*map_data.value, storage_writer);
			}
			index_writer.write_u32(static_cast<u32>(storage_writer.ptr() - storage_base_ptr)); // end
		}
	}

	// end_pos
	BufferWriter{ base_ptr + 3 * sizeof(u32) }.write_u32(static_cast<u32>(storage_writer.ptr() - storage_base_ptr));

	return buffer_size;
}

template<typename Key, typename Value, typename Config>
template<typename Map>
size_t MappedMap<Key, Value, Config>::choose_hash_table_size(const Map& data)
{
	typename Config::KeyHash hasher;

	std::vector<size_t> hashes;
	hashes.reserve(data.size());
	for (const auto& item : data) {
		hashes.push_back(hasher(item.first));
	}

	const size_t max_hash_table_size = static_cast<size_t>(static_cast<double>(data.size()) * 1.2);
	const size_t min_hash_table_size = static_cast<size_t>(static_cast<double>(data.size()) * 0.5);
	std::vector<size_t> prime_numbers = find_prime_numbers(max_hash_table_size);

	const size_t lowest_prime_pos = static_cast<size_t>(
		std::lower_bound(prime_numbers.begin(), prime_numbers.end(), min_hash_table_size)
		- prime_numbers.begin());

	size_t best_hash_table_size = 0;
	int best_collision_count = INT_MAX;
	for (size_t i = lowest_prime_pos; i < prime_numbers.size(); i++) {
		const size_t hash_table_size = prime_numbers[i];
		int collision_count = 0;

		std::vector<int> index_to_count(hash_table_size, 0);
		for (size_t key_hash : hashes) {
			const size_t index = key_hash % hash_table_size;
			index_to_count[index]++;
		}

		for (int count : index_to_count) {
			if (count >= 2) {
				collision_count += count - 1;
			}
		}
		
		if (collision_count < best_collision_count) {
			best_collision_count = collision_count;
			best_hash_table_size = hash_table_size;
		}
	}

	return best_hash_table_size;
}

template<typename Key, typename Value, typename Config>
std::vector<size_t> MappedMap<Key, Value, Config>::find_prime_numbers(size_t n)
{
	std::vector<bool> is_prime(n, true);

	size_t last_prime = 2;
	while (last_prime < n / 2) {
		for (auto j = last_prime * 2; j < n; j += last_prime) {
			is_prime[j] = false;
		}
		last_prime++;
		while (last_prime < n / 2 && !is_prime[last_prime]) {
			last_prime++;
		}
	}

	std::vector<size_t> prime_numbers;
	for (size_t i = 0; i < is_prime.size(); i++) {
		if (is_prime[i]) {
			prime_numbers.push_back(i);
		}
	}
	return prime_numbers;
}

} // namespace bpe
