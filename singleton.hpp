#pragma once

namespace pv {
template <class Ty>
struct Singleton {
  static Ty& GetInstance() noexcept {
    static Ty object;
    return object;
  }
};
}  // namespace pv