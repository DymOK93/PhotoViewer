#pragma once
#include "break_on.hpp"

#include <cassert>
#include <optional>

namespace io {
template <class ConcreteSerializable>
struct Serializable {
  [[nodiscard]] auto Serialize() const noexcept {
    return static_cast<const ConcreteSerializable*>(this)->Serialize();
  }

  [[nodiscard]] static std::optional<ConcreteSerializable> Deserialize(
      const std::byte* raw_data,
      std::size_t bytes_count) noexcept {
    return ConcreteSerializable::Deserialize(raw_data, bytes_count);
  }
};

struct BlockHeader : Serializable<BlockHeader> {
  enum class Category : std::uint8_t { Data = 0x1, Command = 0x2 };
  static constexpr std::size_t MAX_LENGTH{64};

  BlockHeader(Category cat, std::size_t sz) noexcept : category{cat}, size{sz} {
    assert(size != 0 && "invalid block header");
    assert(size <= MAX_LENGTH && "block is too large");
  }

  [[nodiscard]] constexpr std::uint8_t Serialize() const noexcept {
    const std::size_t packed_size{size - 1};
    const auto value{static_cast<std::uint8_t>(category) << 6 | packed_size};
    return static_cast<std::uint8_t>(value);
  }

  [[nodiscard]] static std::optional<BlockHeader> Deserialize(
      const std::byte* raw_data,
      std::size_t bytes_count) noexcept {
    do {
      BREAK_ON_FALSE(bytes_count);

      const auto category{
          static_cast<Category>(static_cast<std::uint8_t>(*raw_data) >> 6)};
      BREAK_ON_FALSE(category == Category::Data ||
                     category == Category::Command);

      const auto block_size{(static_cast<std::uint8_t>(*raw_data) & 0x3F) + 1};
      return BlockHeader{category, static_cast<std::size_t>(block_size)};

    } while (false);

    return std::nullopt;
  }

  Category category;
  std::size_t size;
};
}  // namespace io