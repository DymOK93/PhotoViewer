#include "bmp.hpp"
#include "break_on.hpp"

#include <array>
#include <climits>
#include <cstring>
#include <type_traits>
#include <utility>

using namespace std;

namespace bmp {
namespace details {
template <class Ty,
          std::enable_if_t<
              std::conjunction_v<std::is_nothrow_default_constructible<Ty>,
                                 std::is_trivially_copyable<Ty>>,
              int> = 0>
Ty unaligned_load(const void* from) noexcept {
  Ty value;  // NOLINT(cppcoreguidelines-pro-type-member-init)
  memcpy(addressof(value), from, sizeof(Ty));
  return value;
}
}  // namespace details

Image::Image(const ImageHeader& header) noexcept : m_header{header} {}

optional<Image> Image::FromStream(const byte* stream, size_t size) noexcept {
  do {
    BREAK_ON_FALSE(size == IMAGE_HEADER_RAW_SIZE);

    const auto file_header{load_file_header(stream)};
    BREAK_ON_FALSE(file_header);

    const auto info_header{load_info_header(stream + FILE_HEADER_RAW_SIZE)};
    BREAK_ON_FALSE(info_header);

    const auto bitmap_size{
        static_cast<uint32_t>(info_header->height * info_header->width *
                              info_header->bit_count / CHAR_BIT)};
    BREAK_ON_FALSE(file_header->file_size >= bitmap_size);

    const ImageHeader image_header{*file_header, *info_header};
    return Image{image_header};

  } while (false);

  return nullopt;
}

optional<Image> Image::FromFile(fs::File& file) noexcept {
  array<byte, IMAGE_HEADER_RAW_SIZE> buffer;
  const auto bytes_read{file.Read(data(buffer), IMAGE_HEADER_RAW_SIZE)};
  if (!bytes_read) {
    return nullopt;
  }
  return FromStream(data(buffer), *bytes_read);
}

uint32_t Image::GetWidth() const noexcept {
  return m_header.info.width;
}

uint32_t Image::GetHeight() const noexcept {
  return m_header.info.height;
}

uint32_t Image::GetBitmapOffset() const noexcept {
  return m_header.file.bitmap_offset;
}

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOAD_HEADER_FIELD(field, from, offset) \
  header.field =                               \
      details::unaligned_load<decltype((header).field)>((from) + (offset))

optional<FileHeader> Image::load_file_header(const byte* from) noexcept {
  do {
    FileHeader header{};

    LOAD_HEADER_FIELD(signature, from, 0x0);
    BREAK_ON_FALSE(header.signature == SIGNATURE);

    LOAD_HEADER_FIELD(file_size, from, 0x2);
    BREAK_ON_FALSE(header.file_size > 0);

    LOAD_HEADER_FIELD(bitmap_offset, from, 0xA);
    BREAK_ON_TRUE(header.bitmap_offset < IMAGE_HEADER_RAW_SIZE);

    return header;

  } while (false);

  return nullopt;
}

optional<InfoHeader> Image::load_info_header(const byte* from) noexcept {
  do {
    const auto header{details::unaligned_load<InfoHeader>(from)};

    BREAK_ON_FALSE(header.size);
    BREAK_ON_FALSE(header.width);
    BREAK_ON_FALSE(header.height);

    return header;

  } while (false);

  return nullopt;
}
}  // namespace bmp