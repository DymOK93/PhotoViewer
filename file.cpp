#include "file.hpp"

#include <cstring>

using namespace std;

namespace fs {
LogicalDrive::LogicalDrive(std::optional<std::uint8_t> number,
                           bool delayed_mount) noexcept {
  char drive_name[2]{};
  if (number) {
    drive_name[0] = static_cast<char>(*number);
  }
  if (f_mount(addressof(m_fs), drive_name, !delayed_mount) != FR_OK) {
    m_fs.fs_type = 0;
  }
}

LogicalDrive::~LogicalDrive() noexcept {
  f_mount(nullptr, "", true);
}

bool LogicalDrive::IsMount() const noexcept {
  return m_fs.fs_type != 0;
}

LogicalDrive::operator bool() const noexcept {
  return IsMount();
}

File::File(const char* path, uint8_t flags) noexcept {
  if (f_open(GetHandle(), path, flags) != FR_OK) {
    GetHandle()->obj.fs = nullptr;
  }
}

optional<UINT> File::Read(byte* buffer, UINT bytes_count) noexcept {
  UINT bytes_read;
  if (const auto result =
          f_read(GetHandle(), buffer, bytes_count, addressof(bytes_read));
      result != FR_OK) {
    return nullopt;
  }
  return bytes_read;
}

bool File::Seek(UINT position) noexcept {
  return f_lseek(GetHandle(), position) == FR_OK;
}

bool DirectoryEntry::IsDirectory() const noexcept {
  return m_file_info.fattrib & AM_DIR;
}

bool DirectoryEntry::IsRegularFile() const noexcept {
  return !IsDirectory();
}

const char* DirectoryEntry::Path() const noexcept {
  return m_file_info.fname;
}

const char* DirectoryEntry::Extension() const noexcept {
  const char* dot_pos{strrchr(m_file_info.fname, '.')};
  if (!dot_pos) {
    return EMPTY_EXTENSION;
  }
  return dot_pos + 1;
}

DirectoryEntry::operator const char*() const noexcept {
  return Path();
}

DirectoryIterator::DirectoryIterator(const char* path) {
  if (f_opendir(GetHandle(), path) != FR_OK) {
    GetHandle()->obj.fs = nullptr;
  } else {
    operator++();
  }
}

const DirectoryEntry& DirectoryIterator::operator*() const noexcept {
  return *this;
}

const DirectoryEntry* DirectoryIterator::operator->() const noexcept {
  return this;
}

DirectoryIterator& DirectoryIterator::operator++() noexcept {
  if (auto& info = get_info();
      f_readdir(GetHandle(), addressof(info)) != FR_OK) {
    invalidate();
  }
  return *this;
}

void DirectoryIterator::invalidate() noexcept {
  get_info().fname[0] = '\0';
}

FILINFO& DirectoryIterator::get_info() noexcept {
  return m_file_info;
}

const FILINFO& DirectoryIterator::get_info() const noexcept {
  return m_file_info;
}

bool operator==(const DirectoryIterator& lhs,
                const DirectoryIterator& rhs) noexcept {
  return strcmp(lhs->Path(), rhs->Path()) == 0;
}

bool operator!=(const DirectoryIterator& lhs,
                const DirectoryIterator& rhs) noexcept {
  return !(lhs == rhs);
}

CyclicDirectoryIterator& CyclicDirectoryIterator::operator++() noexcept {
  MyBase::operator++();
  if (auto& info = get_info(); IsOpen() && info.fname[0] == '\0') {
    if (f_readdir(GetHandle(), nullptr) != FR_OK) {
      invalidate();
    } else {
      MyBase::operator++();
    }
  } 
  return *this;
}
}  // namespace fs