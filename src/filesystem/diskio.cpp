#include "sdio.hpp"

#include <tools/attributes.hpp>

// clang-format off
#include <ff.h>
#include <diskio.h>
// clang-format on

#include <cstddef>

using namespace std;

static DSTATUS QuerySdCardStatus() noexcept {
  auto& sd_card{sdio::Card::GetInstance()};
  return sd_card.Ready() ? RES_OK : RES_NOTRDY;
}

EXTERN_C DSTATUS disk_initialize([[maybe_unused]] BYTE pdrv) {
  return QuerySdCardStatus();
}

EXTERN_C DSTATUS disk_status([[maybe_unused]] BYTE pdrv) {
  return QuerySdCardStatus();
}

EXTERN_C DRESULT disk_read([[maybe_unused]] BYTE pdrv,
                           BYTE* buff,
                           LBA_t sector,
                           UINT count) {
  auto& sd_card{sdio::Card::GetInstance()};
  const auto status{sd_card.Read(sector, reinterpret_cast<byte*>(buff), count)};
  return status == sdio::TransferStatus::Success ? RES_OK : RES_ERROR;
}

EXTERN_C DRESULT disk_ioctl([[maybe_unused]] BYTE pdrv,
                            [[maybe_unused]] BYTE cmd,
                            [[maybe_unused]] void* buff) {
  return RES_PARERR;
}