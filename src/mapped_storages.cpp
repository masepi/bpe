#include "mapped_storages.h"

#include <cassert>
#include <fstream>

namespace bpe {

std::vector<u8> load_file_to_buffer(const std::filesystem::path& path)
{
	const size_t file_size = static_cast<size_t>(std::filesystem::file_size(path));
	std::ifstream file{ path, std::ios::binary };
	std::vector<u8> buffer;
	buffer.resize(file_size);
	file.read(reinterpret_cast<char*>(buffer.data()), to<std::streamsize>(file_size));
	return buffer;
}

ShortStringsMappedArray::ShortStringsMappedArray(const u8* data) :
	buffer_size(0),
	element_count(0),
	offsets(nullptr),
	strings(nullptr)
{
	attach(data);
}

ShortStringsMappedArray::ShortStringsMappedArray() :
	buffer_size(0),
	element_count(0),
	offsets(nullptr),
	strings(nullptr)
{
}

size_t ShortStringsMappedArray::attach(const u8* data)
{
	assert(data != nullptr);

	BufferReader reader{ data };

	buffer_size = reader.read_u32();
	element_count = reader.read_u32();
	offsets = data + 2 * sizeof(u32);
	strings = offsets + sizeof(u32) * element_count;

	return buffer_size;
}

size_t ShortStringsMappedArray::write_to_buffer(const std::vector<std::string>& data, std::vector<u8>& buffer)
{
	std::vector<size_t> offsets;
	size_t strings_size = 0;
	for (const auto& item : data) {
		offsets.push_back(strings_size);
		strings_size += item.size() + 1;
	}

	const size_t buffer_size = (2 * sizeof(u32)) + (data.size() * sizeof(u32)) + strings_size;

	const size_t prev_pos = buffer.size();
	buffer.resize(buffer.size() + buffer_size);

	BufferWriter writer{ buffer.data() + prev_pos };

	writer.write_u32(static_cast<u32>(buffer_size));
	writer.write_u32(static_cast<u32>(data.size()));

	for (const auto offset : offsets) {
		writer.write_u32(static_cast<u32>(offset));
	}

	for (const auto& item : data) {
		writer.write_string_view(item);
	}

	return buffer_size;
}

std::string_view ShortStringsMappedArray::operator[](size_t index) const
{
	const size_t offset = BufferReader{ offsets + (sizeof(u32) * index) }.read_u32();
	return BufferReader{ strings + offset }.read_string_view();
}

} // namespace bpe