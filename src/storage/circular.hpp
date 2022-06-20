#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>

namespace storage {
template <size_t Capacity, class Ty>
class CircularBuffer {
  static_assert(std::is_default_constructible_v<Ty>,
                "Ty must be default-constructible");
  static_assert(std::is_nothrow_move_constructible_v<Ty>,
                "Ty must be nothrow-move-constructible");

 public:
  bool produce(Ty value) noexcept {
    return produce_impl(std::addressof(value), 1);
  }

  template <class InputIt>
  std::size_t produce(InputIt in, std::size_t count) noexcept {
    return !count ? 0 : produce_impl(in, count);
  }

  std::optional<Ty> consume() noexcept {
    std::optional<Ty> value;
    consume_impl(1, [&value](Ty* first, Ty* last) {
      if (const auto count = last - first; count != 0) {
        assert(count == 1);
        value = std::move(*first);
      }
    });
    return value;
  }

  template <
      class TransferHandler,
      std::enable_if_t<std::is_invocable_v<TransferHandler, Ty*, Ty*>, int> = 0>
  std::size_t consume(std::size_t count, TransferHandler handler) noexcept {
    return !count ? 0 : consume_impl(count, handler);
  }

  template <class OutputIt>
  bool consume(OutputIt out, std::size_t count) noexcept {
    return consume(count, [&out](Ty* first, Ty* last) {
      out = std::move(first, last, out);
    });
  }

 private:
  template <class InputIt>
  std::size_t produce_impl(InputIt in, std::size_t count) noexcept {
    std::size_t result;
    auto* buffer{std::data(m_buffer)};
    if (const std::size_t head = m_head, tail = m_tail; tail < head) {
      result = std::min(head - tail, count);
      std::move(in, std::next(in, result), buffer + tail);
      m_tail += result;
    } else if (Capacity - tail > count) {
      result = count;
      std::move(in, std::next(in, result), buffer + tail);
      m_tail += result;
    } else {
      const std::size_t remaining{Capacity - tail};
      InputIt middle{std::next(in, remaining)};
      std::move(in, middle, buffer + tail);
      if (head == 0) {
        result = remaining;
        m_tail = Capacity;
      } else {
        result = std::min(count - remaining, head);
        std::move(middle, std::next(middle, result), buffer);
        m_tail = result;
        result += remaining;
      }
    }
    return result;
  }

  template <class TransferHandler>
  std::size_t consume_impl(std::size_t count,
                           TransferHandler handler) noexcept {
    std::size_t result;
    auto* buffer{std::data(m_buffer)};
    if (const std::size_t head = m_head, tail = m_tail; head == tail) {
      result = 0;
    } else if (head < tail) {
      result = std::min(tail - head, count);
      handler(buffer + head, buffer + head + result);
      m_head += result;
    } else if (Capacity - head > count) {
      result = count;
      handler(buffer + head, buffer + head + result);
      m_head += result;
    } else {
      const std::size_t remaining{Capacity - head};
      handler(buffer + head, buffer + head + remaining);
      result = std::min(count - remaining, tail);
      handler(buffer, buffer + result);
      m_head = result;
      result += remaining;
    }
    return result;
  }

 protected:
  std::array<Ty, Capacity> m_buffer;
  volatile std::size_t m_head{0}, m_tail{0};
};
}  // namespace storage