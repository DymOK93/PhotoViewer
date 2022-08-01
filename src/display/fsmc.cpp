#include "fsmc.hpp"

#include <cassert>

namespace fsmc {
volatile std::byte* Controller::BASE_ADDRESS{
    reinterpret_cast<volatile std::byte*>(0x60000000)};

Controller::Controller() noexcept {
  SET_BIT(RCC->AHB3ENR, RCC_AHB3ENR_FSMCEN);
}

volatile std::byte* Controller::calc_bank_address(std::uint8_t bank_number) noexcept {
  assert(bank_number > 0);
  return BASE_ADDRESS + BANK_SIZE * (bank_number - 1);
}
}  // namespace fmc