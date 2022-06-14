#pragma once
#include "circular.hpp"
#include "command.hpp"
#include "io.hpp"
#include "receiver.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace pv {
template <std::size_t CommandQueueDepth, std::size_t DataQueueDepth>
class RequestParser final : public io::IListener {
  template <std::size_t Depth, class Ty>
  using buffer_t = storage::CircularBuffer<Depth, Ty>;

 public:
  using cmd_buffer_t = buffer_t<CommandQueueDepth, cmd::Command>;
  using data_buffer_t = buffer_t<DataQueueDepth, std::byte>;

 public:
  void Process(std::byte value) override {
    if (auto& header = m_current_block; !header) {
      header = deserialize<io::BlockHeader>(value).value();
    } else {
      dispatch(value);
      if (--header->size == 0) {
        header.reset();
      }
    }
  }

  cmd_buffer_t& GetCommands() noexcept { return m_commands; }
  data_buffer_t& GetData() noexcept { return m_data; }

 private:
  void dispatch(std::byte value) noexcept {
    if (m_current_block->category == io::BlockHeader::Category::Data) {
      m_data.produce(value);
    } else {
      m_commands.produce(deserialize<cmd::Command>(value).value());
    }
  }

  template <class Ty>
  static std::optional<Ty> deserialize(std::byte value) noexcept {
    return io::Serializable<Ty>::Deserialize(std::addressof(value), 1);
  }

 private:
  data_buffer_t m_data;
  cmd_buffer_t m_commands;
  std::optional<io::BlockHeader> m_current_block;
};
}  // namespace pv
