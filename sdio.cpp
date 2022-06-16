#include "sdio.hpp"
#include "break_on.hpp"
#include "event.hpp"
#include "gpio.hpp"

#include <cassert>
#include <cstring>
#include <tuple>

using namespace std;

#define PARSE_RESPONSE_CASE(Type)                                   \
  case Type:                                                        \
    response = parse_response_impl<Type>(SDIO->RESP1, SDIO->RESP2,  \
                                         SDIO->RESP3, SDIO->RESP4); \
    break;

extern "C" void EXTI3_IRQHandler() {
  SET_BIT(EXTI->PR, EXTI_PR_PR3);
  sdio::Card::GetInstance().TryAccept();
  NVIC_ClearPendingIRQ(EXTI3_IRQn);
}

namespace sdio {
Card::Card() noexcept {
  prepare_gpio();
  turn_on_pll();
  setup_detection();
}

void Card::SetIAcceptor(IAcceptor* acceptor) {
  m_acceptor = acceptor;
}

void Card::TryAccept() noexcept {
  if (!is_device_present()) {
    set_not_ready();
  } else if (!Ready()) {
    try_get_ready();
  }
}

auto Card::identify_device() noexcept -> optional<DeviceInfo> {
  do {
    const auto go_idle_state{send_command_unchecked<Command::GoIdleState>(0)};
    BREAK_ON_FALSE(go_idle_state);

    const Protocol protocol{recognize_protocol()};
    BREAK_ON_FALSE(protocol != Protocol::Unknown);

    const auto cid_number{send_command_unchecked<Command::SendCardIdNumber>(0)};
    BREAK_ON_FALSE(cid_number);

    const auto rca{send_command_unchecked<Command::SendRelativeAddress>(0)};
    BREAK_ON_FALSE(rca);

    const auto address_for_selection{static_cast<uint32_t>(rca->address << 16)};
    const auto device_id{
        send_command_unchecked<Command::SendCardId>(address_for_selection)};
    BREAK_ON_FALSE(device_id);

    SetClockDivider(DEFAULT_CLOCK_DIVIDER);  // Enter high-speed mode

    auto selection{send_command_unchecked<Command::SelectOrDeselect>(
        address_for_selection)};
    BREAK_ON_FALSE(selection);

    const bool bus_ready{setup_data_bus(address_for_selection)};
    BREAK_ON_FALSE(bus_ready);

    return DeviceInfo{address_for_selection, *device_id};

  } while (false);

  return nullopt;
}

bool Card::Ready() const noexcept {
  return m_device.has_value();
}

auto Card::GetId() const -> const device_id_t& {
  return m_device.value().id;
}

void Card::SetClockDivider(uint8_t divider) const noexcept {
  CLEAR_BIT(SDIO->CLKCR, SDIO_CLKCR_CLKDIV);
  SET_BIT(SDIO->CLKCR, divider);
}

TransferStatus Card::Read(lba_t lba,
                          std::byte* buffer,
                          size_t block_count) const noexcept {
  TransferStatus status{TransferStatus::NotReady};

  do {
    BREAK_ON_FALSE(Ready());
    status = TransferStatus::CommandError;

    uint32_t wait_mask;
    if (block_count == 1) {
      BREAK_ON_FALSE(send_command_unchecked<Command::ReadSingleBlock>(lba));
      wait_mask = SINGLE_BLOCK_READ_WAIT_MASK;
    } else {
      BREAK_ON_FALSE(send_command_unchecked<Command::ReadMultipleBlock>(lba));
      wait_mask = MULTI_BLOCK_READ_WAIT_MASK;
    }

    get_from_fifo(buffer, block_count * BLOCK_SIZE, wait_mask);

    if (block_count > 1) {
      BREAK_ON_FALSE(send_command_unchecked<Command::StopTransmission>(0));
    }

    status = translate_block_io_status(SDIO->STA);

  } while (false);

  return status;
}

TransferStatus Card::Write(lba_t lba,
                           const std::byte* buffer,
                           size_t block_count) const noexcept {
  if (!Ready()) {
    return TransferStatus::NotReady;
  }
  return TransferStatus::CommandError;
}

auto Card::recognize_protocol() noexcept -> Protocol {
  Protocol protocol;
  if (const auto sd_v2 = send_command_unchecked<Command::SendIfCondition>(
          HOST_PROTOCOL_V2_TAG);
      !sd_v2) {
    // v1.0 or unusable card
    protocol = initialize_device(HOST_PROTOCOL_V1_SPECS)
                   ? Protocol::PhysicalSpecV1
                   : Protocol::Unknown;
  } else {
    // v2.0 card
    if (sd_v2->vhs != OPERATION_VOLTAGE ||
        sd_v2->pattern != VOLTAGE_CHECK_PATTERN) {
      protocol = Protocol::Unknown;
    } else {
      protocol = initialize_device(HOST_PROTOCOL_V2_SPECS)
                     ? Protocol::PhysicalSpecV2
                     : Protocol::Unknown;
    }
  }
  return protocol;
}

bool Card::initialize_device(uint32_t host_specs) noexcept {
  for (;;) {
    const auto app_cmd_ready{
        send_command_unchecked<Command::ApplicationSpecific>(0)};
    if (!app_cmd_ready) {
      return false;
    } else if (!app_cmd_ready->app_cmd) {
      continue;
    }

    const auto initialized{
        send_command_unchecked<Command::SendOperationCondition>(host_specs)};
    if (!initialized) {
      return false;
    } else if (!initialized->busy) {
      return true;
    }
  }
}

bool Card::setup_data_bus(uint32_t address_for_selection) noexcept {
  do {
    auto status{send_command_unchecked<Command::SetBlockLen>(BLOCK_SIZE)};
    BREAK_ON_FALSE(status && !status->block_len_error);

    status = send_command_unchecked<Command::ApplicationSpecific>(
        address_for_selection);
    BREAK_ON_FALSE(status);

    status = send_command_unchecked<Command::SetBusWidth>(WIDE_BUS_MODE);
    BREAK_ON_FALSE(status);

    SET_BIT(SDIO->CLKCR, SDIO_CLKCR_WIDBUS_0);

    return true;

  } while (false);

  return false;
}

void Card::set_not_ready() noexcept {
  m_device.reset();
  if (m_acceptor) {
    m_acceptor->Accept(nullptr);
  }
  power_control(false);
}

void Card::try_get_ready() noexcept {
  power_control(true);
  do {
    const auto device_info{identify_device()};
    BREAK_ON_FALSE(device_info);
    BREAK_ON_TRUE(m_acceptor && !m_acceptor->Accept(this));
    m_device = device_info;
    return;
  } while (false);
  power_control(false);
}

void Card::prepare_gpio() noexcept {
  auto& channel_manager{gpio::ChannelManager::GetInstance()};

  auto& gpio_c{channel_manager.Activate<gpio::Channel::C>()};
  SET_BIT(gpio_c.MODER, GPIO_MODER_MODER8_1                // SDIO_D[0]
                            | GPIO_MODER_MODER9_1          // SDIO_D[1]
                            | GPIO_MODER_MODER10_1         // SDIO_D[2]
                            | GPIO_MODER_MODER11_1         // SDIO_D[3]
                            | GPIO_MODER_MODER12_1);       // SDIO_CLK
  SET_BIT(gpio_c.OSPEEDR, GPIO_OSPEEDER_OSPEEDR8           // SDIO_D[0]
                              | GPIO_OSPEEDER_OSPEEDR9     // SDIO_D[1]
                              | GPIO_OSPEEDER_OSPEEDR10    // SDIO_D[2]
                              | GPIO_OSPEEDER_OSPEEDR11    // SDIO_D[3]
                              | GPIO_OSPEEDER_OSPEEDR12);  // SDIO_CLK
  SET_BIT(gpio_c.AFR[1], 0x000CCCCC);

  // SDCardDetect uses default input mode (0x0)
  auto& gpio_d{channel_manager.Activate<gpio::Channel::D>()};
  SET_BIT(gpio_d.MODER, GPIO_MODER_MODER2_1);       // SDIO_CMD
  SET_BIT(gpio_d.OSPEEDR, GPIO_OSPEEDER_OSPEEDR2);  // SDIO_CMD
  SET_BIT(gpio_d.AFR[0], 0x00000C00);               // SDIO_CMD
  SET_BIT(gpio_d.PUPDR, GPIO_PUPDR_PUPDR3_0);       // SDCardDetect
}

void Card::turn_on_pll() noexcept {
  SET_BIT(RCC->PLLCFGR, RCC_PLLCFGR_PLLQ_1);
  SET_BIT(RCC->CR, RCC_CR_PLLON);
  while (!READ_BIT(RCC->CR, RCC_CR_PLLRDY))  // Waiting for PLL readiness
    ;
}

void Card::setup_detection() noexcept {
  auto& exti{event::ExtiManager::GetInstance().Get()};
  SET_BIT(exti.IMR, EXTI_IMR_MR3);
  SET_BIT(exti.RTSR, EXTI_RTSR_TR3);
  SET_BIT(exti.FTSR, EXTI_FTSR_TR3);
  SET_BIT(SYSCFG->EXTICR[0], SYSCFG_EXTICR1_EXTI3_PD);
  NVIC_SetPriority(EXTI3_IRQn, INTERRUPT_PRIORITY);
  NVIC_EnableIRQ(EXTI3_IRQn);
}

bool Card::is_device_present() noexcept {
  auto& gpio{gpio::ChannelManager::GetInstance().Get<gpio::Channel::D>()};
  return !READ_BIT(gpio.IDR, GPIO_IDR_IDR_3);
}

void Card::power_control(bool enable) noexcept {
  if (enable) {
    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_SDIOEN);
    const uint32_t clk_reserved{SDIO->CLKCR & CLOCK_RESERVED_MASK};
    SDIO->CLKCR = clk_reserved | INITIAL_CLOCK_CONFIGURATION;
    SET_BIT(SDIO->POWER, SDIO_POWER_PWRCTRL);  // Power On
  } else {
    CLEAR_BIT(RCC->APB2ENR, RCC_APB2ENR_SDIOEN);
    SET_BIT(RCC->APB2RSTR, RCC_APB2RSTR_SDIORST);
    CLEAR_BIT(RCC->APB2RSTR, RCC_APB2RSTR_SDIORST);
    SDIO->CLKCR &= CLOCK_RESERVED_MASK;
    CLEAR_BIT(SDIO->POWER, SDIO_POWER_PWRCTRL);  // Power Off
  }
}

void Card::send_command_impl(Command command, std::uint32_t arg) noexcept {
  const auto command_idx{static_cast<uint8_t>(command)};
  assert((command_idx & ~SDIO_CMD_CMDINDEX) == 0 && "invalid command");

  SDIO->ICR = CMD_INTERRUPT_CLEAR_MASK;
  SDIO->ARG = arg;
  SDIO->CMD = command_idx | get_native_response_type(command) | SDIO_CMD_CPSMEN;
}

uint16_t Card::get_native_response_type(Command command) noexcept {
  uint16_t native_type;
  switch (command) {
    case Command::GoIdleState:
      native_type = SDIO_CMD_WAITRESP_1;  // No response
      break;
    case Command::SendCardIdNumber:
    case Command::SendCardId:
      native_type = SDIO_CMD_WAITRESP;  // Long response
      break;
    default:
      native_type = SDIO_CMD_WAITRESP_0;  // Short response
      break;
  }
  return native_type;
}

uint32_t Card::get_wait_mask(response::Type type) noexcept {
  return type != response::Type::Empty ? CMD_RESPONSE_WAIT_MASK
                                       : CMD_NO_RESPONSE_WAIT_MASK;
}

uint32_t Card::get_error_mask(response::Type type) noexcept {
  using response::Type;

  uint32_t error_mask;
  switch (type) {
    case Type::CardId:
    case Type::OpCond:
    case Type::CardInfo:
      error_mask = (CMD_ERROR_MASK & ~SDIO_STA_CCRCFAIL);
      break;
    default:
      error_mask = CMD_ERROR_MASK;
      break;
  }
  return error_mask;
}

template <>
response::Empty Card::parse_response([[maybe_unused]] uint32_t r1,
                                     [[maybe_unused]] uint32_t r2,
                                     [[maybe_unused]] uint32_t r3,
                                     [[maybe_unused]] uint32_t r4) noexcept {
  return {};
}

template <>
response::CardStatus Card::parse_response(
    uint32_t r1,
    [[maybe_unused]] uint32_t r2,
    [[maybe_unused]] uint32_t r3,
    [[maybe_unused]] uint32_t r4) noexcept {
  response::CardStatus status{};
  status.out_of_range = (r1 & 0x80000000) >> 31;
  status.address_error = (r1 & 0x40000000) >> 30;
  status.block_len_error = (r1 & 0x20000000) >> 29;
  status.erase_seq_error = (r1 & 0x10000000) >> 28;
  status.erase_param = (r1 & 0x08000000) >> 27;
  status.wp_violation = (r1 & 0x04000000) >> 26;
  status.card_is_locked = (r1 & 0x02000000) >> 25;
  status.lock_unlock_failed = (r1 & 0x01000000) >> 24;
  status.command_crc_error = (r1 & 0x00800000) >> 23;
  status.illegal_command = (r1 & 0x00400000) >> 22;
  status.card_ecc_failed = (r1 & 0x00200000) >> 21;
  status.controller_error = (r1 & 0x00100000) >> 20;
  status.unknown_error = (r1 & 0x00080000) >> 19;
  status.csd_overwrite = (r1 & 0x00010000) >> 16;
  status.wp_erase_skip = (r1 & 0x00008000) >> 15;
  status.card_ecc_disable = (r1 & 0x00004000) >> 14;
  status.erase_reset = (r1 & 0x00002000) >> 13;
  status.current_state = (r1 & 0x00001E00) >> 9;
  status.ready_for_data = (r1 & 0x00000100) >> 8;
  status.fx_event = (r1 & 0x00000040) >> 6;
  status.app_cmd = (r1 & 0x00000020) >> 5;
  status.ake_seq_error = (r1 & 0x00000008) >> 3;
  return status;
}

template <>
response::CardIdNumber Card::parse_response(
    uint32_t r1,
    [[maybe_unused]] uint32_t r2,
    [[maybe_unused]] uint32_t r3,
    [[maybe_unused]] uint32_t r4) noexcept {
  return {r1};
}

template <>
response::RelativeAddress Card::parse_response(
    uint32_t r1,
    [[maybe_unused]] uint32_t r2,
    [[maybe_unused]] uint32_t r3,
    [[maybe_unused]] uint32_t r4) noexcept {
  const auto address{static_cast<uint16_t>((r1 & 0xFFFF0000) >> 16)};
  return {address, static_cast<uint16_t>(r1 & 0xFFFF)};
}

template <>
response::Configuration Card::parse_response(
    uint32_t r1,
    [[maybe_unused]] uint32_t r2,
    [[maybe_unused]] uint32_t r3,
    [[maybe_unused]] uint32_t r4) noexcept {
  response::Configuration config;
  config.spec_version = (r1 & 0x0F000000) >> 24;
  config.spec3 = (r1 & 0x00008000) >> 15;
  config.spec4 = (r1 & 0x00000400) >> 10;
  config.specx = (r1 & 0x000003C0) >> 6;
  config.status_after_erase = (r1 & 0x00800000) >> 23;
  config.security_support = (r1 & 0x00700000) >> 20;
  config.extended_security_support = (r1 & 0x00007800) >> 11;
  config.bus_widths = (r1 & 0x000F0000) >> 16;
  config.cmd_support = (r1 & 0x0000000F);
  return config;
}

template <>
response::OpCond Card::parse_response(uint32_t r1,
                                      [[maybe_unused]] uint32_t r2,
                                      [[maybe_unused]] uint32_t r3,
                                      [[maybe_unused]] uint32_t r4) noexcept {
  response::OpCond ocr;
  ocr.low_voltage = r1 & 0x80;            // bit 7
  ocr.voltage = (r1 & 0xFF8000) >> 15;    // bits [23:15]
  ocr.accepted_1_8v = r1 & 0x1000000;     // bit 24
  ocr.over_2tb_support = r1 & 0x8000000;  // bit 27
  ocr.uhs2 = r1 & 0x20000000;             // bit 29
  ocr.high_capacity = r1 & 0x40000000;    // bit 30
  ocr.busy = (r1 & 0x80000000) == 0;      // bit 31
  return ocr;
}

template <>
response::IfCond Card::parse_response(uint32_t r1,
                                      [[maybe_unused]] uint32_t r2,
                                      [[maybe_unused]] uint32_t r3,
                                      [[maybe_unused]] uint32_t r4) noexcept {
  const auto operation_voltage{static_cast<uint8_t>((r1 & 0xFF00) >> 8)};
  return {operation_voltage, static_cast<uint8_t>(r1 & 0x00FF)};
}

template <>
response::CardId Card::parse_response(uint32_t r1,
                                      uint32_t r2,
                                      uint32_t r3,
                                      uint32_t r4) noexcept {
  response::CardId card_id;

  card_id.manufacturer_id = static_cast<uint8_t>((r1 & 0xFF000000) >> 24);
  card_id.revision = static_cast<uint8_t>((r3 & 0xFF000000) >> 24);

  card_id.oem_id[0] = static_cast<char>((r1 & 0x00FF0000) >> 16);
  card_id.oem_id[1] = static_cast<char>((r1 & 0x0000FF00) >> 8);

  tie(card_id.manufacturing_date.year, card_id.manufacturing_date.month) =
      pair{static_cast<uint8_t>((r4 & 0x000FF000) >> 12),
           static_cast<uint8_t>((r4 & 0x00000F00) >> 8)};

  card_id.product_name[0] = static_cast<char>(r1 & 0x000000FF);
  memcpy(card_id.product_name + 1, addressof(r2), sizeof(uint32_t));

  card_id.serial_number = ((r3 & 0x00FFFFFF) << 8) | ((r4 & 0xFF000000) >> 24);

  return card_id;
}

void Card::get_from_fifo(byte* buffer,
                         size_t bytes_count,
                         uint32_t wait_mask) noexcept {
  assert(bytes_count % FIFO_GRANULARITY == 0 && "invalid data granularity");

  SDIO->DTIMER = READ_TIMEOUT;
  SDIO->DLEN = bytes_count;
  SDIO->ICR = DATA_INTERRPUT_CLEAR_MASK;
  SDIO->DCTRL = SDIO_DCTRL_DTEN | SDIO_DCTRL_DTDIR  // From card to MCU
                | BLOCK_SIZE_FACTOR | SDIO_DCTRL_SDIOEN;

  auto fifo_reader{[buffer]() mutable noexcept {
    const uint32_t value{SDIO->FIFO};
    memcpy(buffer, addressof(value), FIFO_GRANULARITY);
    buffer += FIFO_GRANULARITY;
  }};

  uint32_t status;
  do {
    status = SDIO->STA;
    if (READ_BIT(status, SDIO_STA_RXFIFOF)) {
      for (size_t idx = 0; idx < FIFO_LENGTH; ++idx) {
        fifo_reader();
      }
    }
  } while (!READ_BIT(status, wait_mask));

  while (SDIO->STA & SDIO_STA_RXDAVL) {
    fifo_reader();
  }
}

TransferStatus Card::translate_block_io_status(
    uint32_t native_status) noexcept {
  auto status{TransferStatus::Success};
  if (READ_BIT(native_status, SDIO_STA_DTIMEOUT)) {
    status = TransferStatus::Timeout;
  } else if (READ_BIT(native_status, SDIO_STA_DCRCFAIL)) {
    status = TransferStatus::CrcFail;
  } else if (READ_BIT(native_status, SDIO_STA_RXOVERR)) {
    status = TransferStatus::RxOverrun;
  } else if (READ_BIT(native_status, SDIO_STA_TXUNDERR)) {
    status = TransferStatus::TxUnderrun;
  } else if (READ_BIT(native_status, SDIO_STA_STBITERR)) {
    status = TransferStatus::StartBitError;
  }
  return status;
}
}  // namespace sdio