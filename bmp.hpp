#pragma once
#include "file.hpp"

#include <cstdint>
#include <optional>

namespace bmp {
struct Bgr888 {
  std::uint8_t blue;
  std::uint8_t green;
  std::uint8_t red;
};

struct Rgb666 {
  std::uint16_t red_green;
  std::uint16_t blue;
};

struct FileHeader {
  std::uint16_t signature;
  std::uint32_t file_size;  // NOLINT(clang-diagnostic-padded)
  std::uint16_t reserved[2];
  std::uint32_t bitmap_offset;
};

struct InfoHeader {
  std::uint32_t size;
  std::int32_t width;
  std::int32_t height;
  std::uint16_t planes;
  std::uint16_t bit_count;
};

struct ImageHeader {
  FileHeader file;
  InfoHeader info;
};

class Image {
  static constexpr std::size_t FILE_HEADER_RAW_SIZE{14};
  static constexpr std::size_t INFO_HEADER_RAW_SIZE{sizeof(InfoHeader)};
  static constexpr std::size_t IMAGE_HEADER_RAW_SIZE{FILE_HEADER_RAW_SIZE +
                                                     INFO_HEADER_RAW_SIZE};
  static constexpr std::uint16_t SIGNATURE{0x4d42};

 public:
  static std::optional<Image> FromStream(const std::byte* stream,
                                         std::size_t size) noexcept;
  static std::optional<Image> FromFile(fs::File& file) noexcept;

  [[nodiscard]] std::uint32_t GetWidth() const noexcept;
  [[nodiscard]] std::uint32_t GetHeight() const noexcept;
  [[nodiscard]] std::uint32_t GetBitmapOffset() const noexcept;

 private:
  Image(const ImageHeader& header) noexcept;

  static std::optional<FileHeader> load_file_header(
      const std::byte* from) noexcept;

  static std::optional<InfoHeader> load_info_header(
      const std::byte* from) noexcept;

 private:
  ImageHeader m_header;
};
}  // namespace bmp