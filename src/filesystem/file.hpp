#pragma once
#include <ff.h>

#include <iterator>
#include <optional>
#include <utility>

namespace fs {
class LogicalDrive {
 public:
  explicit LogicalDrive(std::optional<std::uint8_t> number,
                        bool delayed_mount = true) noexcept;

  LogicalDrive(const LogicalDrive&) = delete;
  LogicalDrive(LogicalDrive&&) = delete;
  LogicalDrive& operator=(const LogicalDrive&) = delete;
  LogicalDrive& operator=(LogicalDrive&&) = delete;
  ~LogicalDrive() noexcept;

  [[nodiscard]] bool IsMount() const noexcept;
  explicit operator bool() const noexcept;

 private:
  FATFS m_fs{};
};

namespace details {
template <class Ty>
struct CloseHelper;

template <>
struct CloseHelper<FIL> {
  void operator()(FIL* handle) const noexcept { f_close(handle); }
};

template <>
struct CloseHelper<DIR> {
  void operator()(DIR* handle) const noexcept { f_closedir(handle); }
};

template <class Ty>
class FileBase {
 public:
  constexpr explicit FileBase() = default;

  FileBase(const FileBase&) noexcept = delete;

  constexpr FileBase(FileBase&& other) noexcept : m_entry{*other.GetHandle()} {
    get_fs(other.m_entry) = nullptr;
  }

  FileBase& operator=(const FileBase&) noexcept = delete;

  FileBase& operator=(FileBase&& other) noexcept {
    if (std::addressof(other) != this) {
      Reset(other.GetHandle());
      get_fs(other.m_entry) = nullptr;
    }
    return *this;
  }

  ~FileBase() noexcept { Reset(); }

  constexpr Ty* GetHandle() noexcept { return std::addressof(m_entry); }

  void Reset(Ty* handle = nullptr) noexcept {
    if (IsOpen()) {
      CloseHelper<Ty>{}(GetHandle());
    }
    if (!handle) {
      get_fs(m_entry) = nullptr;
    } else {
      m_entry = *handle;
    }
  }

  [[nodiscard]] constexpr bool IsOpen() const noexcept {
    return get_fs(m_entry);
  }

  constexpr explicit operator bool() const noexcept { return IsOpen(); }

 private:
  template <class Entry>
  static decltype(auto) get_fs(Entry& entry) noexcept {
    auto& as_ref{entry.obj.fs};
    return as_ref;
  }

 private:
  Ty m_entry{};
};
}  // namespace details

class File : details::FileBase<FIL> {
  using MyBase = FileBase;

 public:
  constexpr explicit File() = default;
  explicit File(const char* path, std::uint8_t flags) noexcept;

  using MyBase::IsOpen;
  using MyBase::operator bool;

  std::optional<UINT> Read(std::byte* buffer, UINT bytes_count) noexcept;
  bool Seek(UINT position) noexcept;
};

class DirectoryEntry {
  static constexpr auto* EMPTY_EXTENSION{""};

 public:
  [[nodiscard]] bool IsDirectory() const noexcept;
  [[nodiscard]] bool IsRegularFile() const noexcept;

  [[nodiscard]] const char* Path() const noexcept;
  operator const char*() const noexcept;

  [[nodiscard]] const char* Extension() const noexcept;

 protected:
  FILINFO m_file_info{};
  mutable const char* m_extension{nullptr};
};

class DirectoryIterator : protected details::FileBase<DIR>, DirectoryEntry {
 public:
  constexpr DirectoryIterator() = default;
  explicit DirectoryIterator(const char* path);

  const DirectoryEntry& operator*() const noexcept;
  const DirectoryEntry* operator->() const noexcept;
  DirectoryIterator& operator++() noexcept;

 protected:
  void invalidate() noexcept;

  [[nodiscard]] FILINFO& get_info() noexcept;
  [[nodiscard]] const FILINFO& get_info() const noexcept;
};

bool operator==(const DirectoryIterator& lhs,
                const DirectoryIterator& rhs) noexcept;
bool operator!=(const DirectoryIterator& lhs,
                const DirectoryIterator& rhs) noexcept;

struct CyclicDirectoryIterator : DirectoryIterator {
  using MyBase = DirectoryIterator;

  using MyBase::MyBase;

  CyclicDirectoryIterator& operator++() noexcept;
};
}  // namespace fs