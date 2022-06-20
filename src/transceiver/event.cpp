#include "event.hpp"

namespace event {
exti_t& ExtiManager::Get() {
  return *EXTI;
}

ExtiManager::ExtiManager() noexcept {
  SET_BIT(RCC->APB2ENR, RCC_APB2ENR_SYSCFGEN | RCC_APB2ENR_EXTIEN);
}
}  // namespace event