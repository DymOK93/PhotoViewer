#pragma once
#include "meta.hpp"
#include "singleton.hpp"

#include <cstdint>
#include <optional>

#include <stm32f4xx.h>

namespace sdio {
enum class Command : std::uint8_t {
  GoIdleState = 0,
  SendCardIdNumber = 2,
  SendRelativeAddress = 3,
  SetBusWidth = 6,
  SelectOrDeselect = 7,
  SendIfCondition = 8,
  SendCardId = 10,
  StopTransmission = 12,
  SetBlockLen = 16,
  ReadSingleBlock = 17,
  ReadMultipleBlock = 18,
  SendOperationCondition = 41,
  SendConfiguration = 51,
  ApplicationSpecific = 55
};

enum class TransferStatus : std::int8_t {
  NotReady = -1,
  Success,
  CommandError,
  Timeout,
  CrcFail,
  RxOverrun,
  TxUnderrun,
  StartBitError,
};

using lba_t = uint32_t;

namespace response {
enum class Type : std::uint8_t {
  Empty,
  CardStatus,
  ExtendedCardStatus,
  CardIdNumber,
  Configuration,
  CardId,
  CardData,
  OpCond,
  FastIo,
  CardInfo,
  RelativeAddress,
  IfCond
};

struct Empty {};

struct CardStatus {
  std::uint32_t out_of_range : 1;
  std::uint32_t address_error : 1;
  std::uint32_t block_len_error : 1;
  std::uint32_t erase_seq_error : 1;
  std::uint32_t erase_param : 1;
  std::uint32_t wp_violation : 1;
  std::uint32_t card_is_locked : 1;
  std::uint32_t lock_unlock_failed : 1;
  std::uint32_t command_crc_error : 1;
  std::uint32_t illegal_command : 1;
  std::uint32_t card_ecc_failed : 1;
  std::uint32_t controller_error : 1;
  std::uint32_t unknown_error : 1;
  std::uint32_t csd_overwrite : 1;
  std::uint32_t wp_erase_skip : 1;
  std::uint32_t card_ecc_disable : 1;
  std::uint32_t erase_reset : 1;
  std::uint32_t current_state : 4;
  std::uint32_t ready_for_data : 1;
  std::uint32_t fx_event : 1;
  std::uint32_t app_cmd : 1;
  std::uint32_t ake_seq_error : 1;
};

struct ExtendedCardStatus : CardStatus {
  bool busy;
};

struct CardIdNumber {
  std::uint32_t value;
};

struct Configuration {
  std::uint32_t spec_version : 4;
  std::uint32_t spec3 : 1;
  std::uint32_t spec4 : 1;
  std::uint32_t specx : 4;
  std::uint32_t status_after_erase : 1;
  std::uint32_t security_support : 3;
  std::uint32_t extended_security_support : 4;
  std::uint32_t bus_widths : 4;
  std::uint32_t cmd_support : 4;
};

struct Timestamp {
  uint8_t year;
  uint8_t month;
};

struct CardId {
  std::uint8_t manufacturer_id;
  std::uint8_t revision;
  char oem_id[2];
  Timestamp manufacturing_date;
  char product_name[5];
  std::uint32_t serial_number;
};

struct CardData {};

struct OpCond {
  std::uint32_t low_voltage : 1;
  std::uint32_t voltage : 9;
  std::uint32_t accepted_1_8v : 1;
  std::uint32_t over_2tb_support : 1;
  std::uint32_t uhs2 : 1;
  std::uint32_t high_capacity : 1;
  std::uint32_t busy : 1;
};

struct FastIo {
  std::uint16_t rca;
  std::uint8_t reg_address;
  std::uint8_t reg_contents;
};

struct CardInfo {
  std::uint16_t ready;
  std::uint8_t io_functions;
  bool present_memory;
  std::uint8_t stuff;
  std::uint8_t io_orc[3];
};

struct RelativeAddress {
  std::uint16_t address;
  std::uint16_t irq_data;
};

struct IfCond {
  std::uint8_t vhs;
  std::uint8_t pattern;
};

using type_list_t = meta::TypeList<Empty,
                                   CardStatus,
                                   ExtendedCardStatus,
                                   CardIdNumber,
                                   Configuration,
                                   CardId,
                                   CardData,
                                   OpCond,
                                   FastIo,
                                   CardInfo,
                                   RelativeAddress,
                                   IfCond>;

static constexpr Type MapCommandToResponse(Command command) noexcept {
  Type type{};
  switch (command) {
    case Command::SetBusWidth:
    case Command::SelectOrDeselect:
    case Command::StopTransmission:
    case Command::SetBlockLen:
    case Command::ReadSingleBlock:
    case Command::ReadMultipleBlock:
    case Command::ApplicationSpecific:
      type = Type::CardStatus;
      break;
    case Command::SendCardIdNumber:
      type = Type::CardIdNumber;
      break;
    case Command::SendConfiguration:
      type = Type::Configuration;
      break;
    case Command::SendCardId:
      type = Type::CardId;
      break;
    case Command::SendOperationCondition:
      type = Type::OpCond;
      break;
    case Command::SendRelativeAddress:
      type = Type::RelativeAddress;
      break;
    case Command::SendIfCondition:
      type = Type::IfCond;
      break;
    default:
      type = Type::Empty;
      break;
  }
  return type;
}

template <Type Idx>
using type_by_index_t =
    typename meta::TypeByIndex<static_cast<size_t>(Idx), type_list_t>::type;

template <Command Cmd>
using type_by_command_t = type_by_index_t<MapCommandToResponse(Cmd)>;
}  // namespace response

class Card;

struct IAcceptor {
  IAcceptor() = default;
  IAcceptor(const IAcceptor&) = default;
  IAcceptor(IAcceptor&&) = default;
  IAcceptor& operator=(const IAcceptor&) = default;
  IAcceptor& operator=(IAcceptor&&) = default;
  virtual ~IAcceptor() = default;

  virtual bool Accept(Card*) = 0;
};

class Card : public pv::Singleton<Card> {
  enum class Protocol { Unknown, PhysicalSpecV1, PhysicalSpecV2 };

  static constexpr std::uint8_t INTERRUPT_PRIORITY{13};
  static constexpr std::uint32_t SDIO_CLOCK{48'000'000};
  static constexpr std::uint32_t INITIAL_CLOCK{400'000};
  static constexpr std::uint32_t INITIAL_CLOCK_DIVIDER{SDIO_CLOCK /
                                                       INITIAL_CLOCK};
  static constexpr std::uint32_t DEFAULT_CLOCK_DIVIDER{0};
  static constexpr std::uint32_t INITIAL_CLOCK_CONFIGURATION{
      INITIAL_CLOCK_DIVIDER | SDIO_CLKCR_PWRSAV | SDIO_CLKCR_CLKEN |
      SDIO_CLKCR_HWFC_EN};
  static constexpr uint32_t CLOCK_RESERVED_MASK{0xFF800000};

  static constexpr std::uint32_t CMD_ERROR_MASK{SDIO_STA_CCRCFAIL |
                                                SDIO_STA_CTIMEOUT};
  static constexpr std::uint32_t CMD_RESPONSE_WAIT_MASK{CMD_ERROR_MASK |
                                                        SDIO_STA_CMDREND};
  static constexpr std::uint32_t CMD_NO_RESPONSE_WAIT_MASK{CMD_ERROR_MASK |
                                                           SDIO_STA_CMDSENT};
  static constexpr std::uint32_t CMD_INTERRUPT_CLEAR_MASK{
      SDIO_ICR_CCRCFAILC | SDIO_ICR_CTIMEOUTC | SDIO_ICR_CMDRENDC |
      SDIO_ICR_CMDSENTC};

  static constexpr std::uint8_t OPERATION_VOLTAGE{0b0001};
  static constexpr std::uint8_t VOLTAGE_CHECK_PATTERN{0b10101010};
  static constexpr std::uint32_t HOST_PROTOCOL_V2_TAG{OPERATION_VOLTAGE << 8 |
                                                      VOLTAGE_CHECK_PATTERN};

  // SDSC (V1) or SDHC (V2) support, 3.2-3.4V, PowerSaving
  static constexpr std::uint32_t HOST_VOLTAGE_WINDOW{0x300000};
  static constexpr std::uint32_t HOST_PROTOCOL_V1_SPECS{HOST_VOLTAGE_WINDOW |
                                                        0};
  static constexpr std::uint32_t HOST_PROTOCOL_V2_SPECS{HOST_VOLTAGE_WINDOW |
                                                        0x40000000};
  static constexpr std::uint32_t BLOCK_SIZE{512};
  static constexpr std::uint32_t BLOCK_SIZE_FACTOR{SDIO_DCTRL_DBLOCKSIZE_0 |
                                                   SDIO_DCTRL_DBLOCKSIZE_3};
  static constexpr std::uint32_t WIDE_BUS_MODE{0b10};

  static constexpr std::uint32_t DATA_INTERRPUT_CLEAR_MASK{
      SDIO_ICR_DCRCFAILC | SDIO_ICR_DTIMEOUTC | SDIO_ICR_TXUNDERRC |
      SDIO_ICR_RXOVERRC | SDIO_ICR_DATAENDC | SDIO_ICR_DBCKENDC};

  static constexpr std::uint32_t FIFO_GRANULARITY{4};
  static constexpr std::uint32_t FIFO_LENGTH{16};

  static constexpr std::uint32_t IO_WAIT_MASK{
      SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_STBITERR};

  static constexpr std::uint32_t SINGLE_BLOCK_IO_WAIT_MASK{IO_WAIT_MASK |
                                                           SDIO_STA_DBCKEND};
  static constexpr std::uint32_t SINGLE_BLOCK_READ_WAIT_MASK{
      SINGLE_BLOCK_IO_WAIT_MASK | SDIO_STA_RXOVERR};
  static constexpr std::uint32_t SINGLE_BLOCK_WRITE_WAIT_MASK{
      SINGLE_BLOCK_IO_WAIT_MASK | SDIO_STA_TXUNDERR};

  static constexpr std::uint32_t MULTI_BLOCK_IO_WAIT_MASK{IO_WAIT_MASK |
                                                          SDIO_STA_DATAEND};
  static constexpr std::uint32_t MULTI_BLOCK_READ_WAIT_MASK{
      MULTI_BLOCK_IO_WAIT_MASK | SDIO_STA_RXOVERR};
  static constexpr std::uint32_t MULTI_BLOCK_WRITE_ERROR_MASK{
      MULTI_BLOCK_IO_WAIT_MASK | SDIO_STA_TXUNDERR};

  static constexpr std::uint32_t DATA_TIMEOUT_GRANULARITY{
      SDIO_CLOCK / (DEFAULT_CLOCK_DIVIDER + 2) / 1000};  // 1 ms
  static constexpr std::uint32_t READ_TIMEOUT{DATA_TIMEOUT_GRANULARITY * 100};
  static constexpr std::uint32_t WRITE_TIMEOUT{DATA_TIMEOUT_GRANULARITY * 250};

 public:
  template <Command Cmd>
  using command_response_t = std::optional<response::type_by_command_t<Cmd>>;

  using device_id_t = response::CardId;

 private:
  struct DeviceInfo {
    std::uint32_t address;
    device_id_t id;
  };

 public:
  void SetIAcceptor(IAcceptor* acceptor) noexcept;
  void TryAccept() noexcept;

  template <Command Cmd>
  [[nodiscard]] command_response_t<Cmd> SendCommand(
      std::uint32_t arg) const noexcept {
    if (!Ready()) {
      return std::nullopt;
    }
    return send_command_unchecked<Cmd>(arg);
  }

  [[nodiscard]] bool Ready() const noexcept;
  [[nodiscard]] const device_id_t& GetId() const;

  void SetClockDivider(uint8_t divider) const noexcept;

  [[nodiscard]] TransferStatus Read(lba_t lba,
                                    std::byte* buffer,
                                    std::size_t block_count) const noexcept;

  [[nodiscard]] TransferStatus Write(lba_t lba,
                                     const std::byte* buffer,
                                     std::size_t block_count) const noexcept;

 private:
  friend Singleton;

  Card() noexcept;

  template <Command Cmd>
  [[nodiscard]] command_response_t<Cmd> send_command_unchecked(
      std::uint32_t arg) const noexcept {
    constexpr auto response_type{response::MapCommandToResponse(Cmd)};
    send_command_impl(Cmd, arg);
    while (!READ_BIT(SDIO->STA, get_wait_mask(response_type)))
      ;
    if (READ_BIT(SDIO->STA, get_error_mask(response_type))) {
      return std::nullopt;
    }
    return parse_response<response::type_by_index_t<response_type>>(
        SDIO->RESP1, SDIO->RESP2, SDIO->RESP3, SDIO->RESP4);
  }

  std::optional<DeviceInfo> identify_device() noexcept;
  Protocol recognize_protocol() noexcept;
  bool initialize_device(uint32_t host_specs) noexcept;
  bool setup_data_bus(uint32_t address_for_selection) noexcept;

  void set_not_ready() noexcept;
  void try_get_ready() noexcept;

  static void prepare_gpio() noexcept;
  static void turn_on_pll() noexcept;
  static void setup_detection() noexcept;

  static bool is_device_present() noexcept;
  static void power_control(bool enable) noexcept;

  static void send_command_impl(Command command, std::uint32_t arg) noexcept;

  static std::uint16_t get_native_response_type(Command command) noexcept;
  static std::uint32_t get_wait_mask(response::Type type) noexcept;
  static std::uint32_t get_error_mask(response::Type type) noexcept;

  template <class Ty>
  static Ty parse_response(std::uint32_t r1,
                           std::uint32_t r2,
                           std::uint32_t r3,
                           std::uint32_t r4) noexcept;

  static void get_from_fifo(std::byte* buffer,
                            size_t bytes_count,
                            uint32_t wait_mask) noexcept;

  static TransferStatus translate_block_io_status(
      uint32_t native_status) noexcept;

 private:
  IAcceptor* m_acceptor{nullptr};
  std::optional<DeviceInfo> m_device;
};
}  // namespace sdio
