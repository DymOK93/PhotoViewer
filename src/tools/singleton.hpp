#pragma once

namespace pv {
template <class Ty>
class Singleton {
public:
  Singleton(const Singleton&) = delete;
  Singleton(Singleton&&) = delete;
  Singleton& operator=(const Singleton&) = delete;
  Singleton& operator=(Singleton&&) = delete;
  ~Singleton() = default;

  static Ty& GetInstance() noexcept {
    static Ty object;
    return object;
  }

protected:
  Singleton() = default;
};
}  // namespace pv