#include <parsex/schema/atomic_write.hpp>
#include <parsex/schema/detail/atomic_write_detail.hpp>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <iomanip>
#include <random>
#include <sstream>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>      // _SH_DENYWR (MinGW doesn't expose it via <io.h>)
#include <sys/stat.h>   // _S_IREAD/_S_IWRITE (ditto)
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

[[noreturn]] void throwErrno(const std::filesystem::path& path, const char* what) {
    throw std::system_error(
        std::error_code(errno, std::generic_category()),
        std::string(what) + ": " + path.string());
}

std::string randomHexSuffix() {
    std::random_device randomDevice;
    std::mt19937_64 generator(randomDevice());
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << generator();
    return out.str();
}

int portableOpen(const std::filesystem::path& tmp) {
#ifdef _WIN32
    int fileDesc = -1;
    _sopen_s(&fileDesc, tmp.string().c_str(),
             _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _SH_DENYWR,
             _S_IREAD | _S_IWRITE);
    return fileDesc;
#else
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): open() with a mode argument is inherently variadic.
    return ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
#endif
}

bool portableWriteFull(int fileDesc, const std::string& data) {
    std::size_t done = 0;
    while (done < data.size()) {
#ifdef _WIN32
        const int chunk =
            static_cast<int>((std::min)(data.size() - done, std::size_t{INT_MAX}));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): fd I/O needs a raw offset pointer.
        const int writeCount = ::_write(fileDesc, data.data() + done, static_cast<unsigned>(chunk));
        if (writeCount < 0) {
            return false;
        }
        done += static_cast<std::size_t>(writeCount);
#else
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): fd I/O needs a raw offset pointer.
        const ssize_t writeCount = ::write(fileDesc, data.data() + done, data.size() - done);
        if (writeCount < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        done += static_cast<std::size_t>(writeCount);
#endif
    }
    return true;
}

bool portableSync(int fileDesc) {
#ifdef _WIN32
    return ::_commit(fileDesc) == 0;
#else
    return ::fsync(fileDesc) == 0;
#endif
}

void portableClose(int fileDesc) {
#ifdef _WIN32
    ::_close(fileDesc);
#else
    ::close(fileDesc);
#endif
}

// Removes tmp unless disarmed (i.e. unless rename() already succeeded).
class TmpCleanup {
public:
    explicit TmpCleanup(std::filesystem::path tmp) : tmp_(std::move(tmp)) {}
    ~TmpCleanup() {
        if (armed_) {
            std::error_code removeError;
            std::filesystem::remove(tmp_, removeError);
        }
    }
    TmpCleanup(const TmpCleanup&) = delete;
    TmpCleanup& operator=(const TmpCleanup&) = delete;
    TmpCleanup(TmpCleanup&&) noexcept = default;
    TmpCleanup& operator=(TmpCleanup&&) noexcept = default;
    void disarm() { armed_ = false; }

private:
    std::filesystem::path tmp_;
    bool armed_ = true;
};

}  // namespace

void writeCacheAtomically(const std::filesystem::path& path, const std::string& data) {
    std::filesystem::path dir = path.parent_path();
    if (dir.empty()) {
        dir = ".";
    }
    std::error_code dirError;
    std::filesystem::create_directories(dir, dirError);
    if (dirError) {
        throw std::system_error(dirError, "writeCacheAtomically: create_directories: " + dir.string());
    }

    const std::filesystem::path tmp =
        detail::makeTmpPath(dir, path.filename().string());
    TmpCleanup cleanup(tmp);

    detail::writeTmpFileSync(tmp, data);

    // Atomic on POSIX when tmp and path share a filesystem (same directory
    // guarantees it); replaces any existing entry atomically.
    std::filesystem::rename(tmp, path, dirError);
    if (dirError) {
        throw std::system_error(dirError, "writeCacheAtomically: rename: " + tmp.string());
    }
    cleanup.disarm();
}

namespace detail {

std::filesystem::path makeTmpPath(
    const std::filesystem::path& dir, const std::string& filename) {
    return dir / (filename + ".tmp." + randomHexSuffix());
}

void writeTmpFileSync(const std::filesystem::path& tmpPath, const std::string& data) {
    const int fileDesc = portableOpen(tmpPath);
    if (fileDesc < 0) {
        throwErrno(tmpPath, "writeTmpFileSync: open");
    }
    if (!portableWriteFull(fileDesc, data)) {
        const int writeErr = errno;
        portableClose(fileDesc);
        errno = writeErr;
        throwErrno(tmpPath, "writeTmpFileSync: write");
    }
    if (!portableSync(fileDesc)) {
        const int syncErr = errno;
        portableClose(fileDesc);
        errno = syncErr;
        throwErrno(tmpPath, "writeTmpFileSync: fsync");
    }
    portableClose(fileDesc);
}

}  // namespace detail
