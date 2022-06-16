#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <type_traits>

namespace storage {
template <size_t Capacity, class Ty>
class CircularQueue {
  static_assert(std::is_default_constructible_v<Ty>,
                "Ty must be default-constructible");
  static_assert(std::is_nothrow_move_constructible_v<Ty>,
                "Ty must be nothrow-move-constructible");

 public:
  void push(Ty value) noexcept {
    m_buffer[shift_index(m_tail)] = std::move(value);
    m_size = m_size + 1;
  }

  Ty pop() noexcept {
    assert(!empty());
    auto value{std::move(m_buffer[shift_index(m_head)])};
    m_size = m_size - 1;
    return value;
  }

  [[nodiscard]] bool empty() const noexcept { return m_size == 0; }
  [[nodiscard]] std::size_t size() const noexcept { return m_size; }

 private:
  static std::size_t shift_index(volatile std::size_t& idx) noexcept {
    const std::size_t prev{idx};
    idx = prev + 1 == Capacity ? 0 : idx + 1;
    return prev;
  }

 protected:
  Ty m_buffer[Capacity]{};
  volatile std::size_t m_head{0}, m_tail{0};
  volatile std::size_t m_size{0};
};

template <size_t Capacity, class Ty>
class CircularBuffer : CircularQueue<Capacity, Ty> {
  using MyBase = CircularQueue<Capacity, Ty>;

 public:
  void produce(Ty value) noexcept { MyBase::push(value); }

  template <class InputIt>
  bool produce(InputIt in, std::size_t count) noexcept {
    if (!count || this->m_size + count > Capacity) {
      return false;
    }
    this->m_size = this->m_size + count;
    produce_impl(in, count);
    return true;
  }

  Ty consume() noexcept { return MyBase::pop(); }

  template <
      class TransferHandler,
      std::enable_if_t<std::is_invocable_v<TransferHandler, Ty*, Ty*>, int> = 0>
  bool consume(std::size_t count, TransferHandler handler) noexcept {
    if (!count || this->m_size < count) {
      return false;
    }
    this->m_size = this->m_size - count;
    consume_impl(count, handler);
    return true;
  }

  template <class OutputIt>
  bool consume(OutputIt out, std::size_t count) noexcept {
    return consume(count, [&out](Ty* first, Ty* last) {
      out = std::move(first, last, out);
    });
  }

  using MyBase::empty;
  using MyBase::size;

 private:
  template <class InputIt>
  void produce_impl(InputIt in, std::size_t count) noexcept {
    if (const std::size_t tail = this->m_tail;
        tail < this->m_head || Capacity - tail > count) {
      std::move(in, std::next(in, count), this->m_buffer + tail);
      this->m_tail = tail + count;
    } else {
      const std::size_t remaining{Capacity - tail};
      const auto middle{std::next(in, remaining)};
      std::move(in, middle, this->m_buffer + tail);
      count -= remaining;
      std::move(middle, std::next(middle, count), this->m_buffer);
      this->m_tail = count;
    }
  }

  template <class TransferHandler>
  void consume_impl(std::size_t count, TransferHandler handler) noexcept {
    if (const std::size_t head = this->m_head;
        head < this->m_tail || Capacity - head > count) {
      Ty* middle{this->m_buffer + head};
      handler(middle, middle + count);
      this->m_head = head + count;
    } else {
      const std::size_t remaining{Capacity - head};
      Ty* middle{this->m_buffer + head};
      handler(middle, middle + remaining);
      count -= remaining;
      handler(this->m_buffer, this->m_buffer + count);
      this->m_head = count;
    }
  }
};
}  // namespace storage