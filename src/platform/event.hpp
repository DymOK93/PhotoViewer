#pragma once
#include <tools/singleton.hpp>

#include <stm32f4xx.h>

namespace event {
using exti_t = EXTI_TypeDef;

class ExtiManager : public pv::Singleton<ExtiManager> {
 public:
  exti_t& Get();

 private:
  friend Singleton;

  ExtiManager() noexcept;
};
}  // namespace event