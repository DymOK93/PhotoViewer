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
    ++m_size;
  }

  Ty pop() noexcept {
    assert(!empty());
    auto value{std::move(m_buffer[shift_index(m_head)])};
    --m_size;
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
  bool produce(InputIt first, std::size_t count) noexcept {
    if (!count || this->m_size + count > Capacity) {
      return false;
    }
    produce_impl(first, count);
    this->m_size += count;
    return true;
  }

  Ty consume() noexcept { return MyBase::pop(); }

  template <class OutputIt>
  bool consume(OutputIt first, std::size_t count) noexcept {
    if (!count || this->m_size < count) {
      return false;
    }
    consume_impl(first, count);
    this->m_size -= count;
    return true;
  }

  using MyBase::empty;
  using MyBase::size;

 private:
  template <class InputIt>
  void produce_impl(InputIt first, std::size_t count) noexcept {
    if (const std::size_t head = this->m_head, tail = this->m_tail;
        tail <= head || Capacity - tail > count) {
      std::move(first, std::next(first, count), this->m_buffer + tail);
      this->m_tail += count;
    } else {
      const std::size_t remaining{Capacity - tail};
      const auto middle{std::next(first, remaining)};
      std::move(first, middle, this->m_buffer + tail);
      count -= remaining;
      std::move(middle, std::next(middle, count), this->m_buffer);
      this->m_head = count;
    }
  }

  template <class OutputIt>
  void consume_impl(OutputIt first, std::size_t count) noexcept {
    if (const std::size_t head = this->m_head, tail = this->m_tail;
        tail > head || Capacity - head > count) {
      const Ty* middle{this->m_buffer + head};
      std::move(middle, middle + count, first);
      this->m_head += count;
    } else {
      const std::size_t remaining{Capacity - head};
      const Ty* middle{this->m_buffer + head};
      std::move(middle + head, middle + remaining, first);
      count -= remaining;
      std::move(this->m_buffer, this->m_buffer + count,
                std::next(first, remaining));
      this->m_head = count;
    }
  }
};
}  // namespace storage