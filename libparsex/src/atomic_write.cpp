#include <parsex/schema/atomic_write.hpp>
#include <parsex/schema/detail/atomic_write_detail.hpp>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <random>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
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
    std::random_device rd;
    std::mt19937_64 gen(rd());
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(gen()));
    return std::string(buf);
}

int portableOpen(const std::filesystem::path& tmp) {
#ifdef _WIN32
    int fd = -1;
    _sopen_s(&fd, tmp.string().c_str(),
             _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _SH_DENYWR,
             _S_IREAD | _S_IWRITE);
    return fd;
#else
    return ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
#endif
}

bool portableWriteFull(int fd, const std::string& data) {
    std::size_t done = 0;
    while (done < data.size()) {
#ifdef _WIN32
        const int chunk =
            static_cast<int>((std::min)(data.size() - done, std::size_t{INT_MAX}));
        const int n = ::_write(fd, data.data() + done, static_cast<unsigned>(chunk));
        if (n < 0) {
            return false;
        }
        done += static_cast<std::size_t>(n);
#else
        const ssize_t n = ::write(fd, data.data() + done, data.size() - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        done += static_cast<std::size_t>(n);
#endif
    }
    return true;
}

bool portableSync(int fd) {
#ifdef _WIN32
    return ::_commit(fd) == 0;
#else
    return ::fsync(fd) == 0;
#endif
}

void portableClose(int fd) {
#ifdef _WIN32
    ::_close(fd);
#else
    ::close(fd);
#endif
}

// Removes tmp unless disarmed (i.e. unless rename() already succeeded).
class TmpCleanup {
public:
    explicit TmpCleanup(std::filesystem::path tmp) : tmp_(std::move(tmp)) {}
    ~TmpCleanup() {
        if (armed_) {
            std::error_code ec;
            std::filesystem::remove(tmp_, ec);
        }
    }
    TmpCleanup(const TmpCleanup&) = delete;
    TmpCleanup& operator=(const TmpCleanup&) = delete;
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
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        throw std::system_error(ec, "writeCacheAtomically: create_directories: " + dir.string());
    }

    const std::filesystem::path tmp =
        detail::makeTmpPath(dir, path.filename().string());
    TmpCleanup cleanup(tmp);

    detail::writeTmpFileSync(tmp, data);

    // Atomic on POSIX when tmp and path share a filesystem (same directory
    // guarantees it); replaces any existing entry atomically.
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        throw std::system_error(ec, "writeCacheAtomically: rename: " + tmp.string());
    }
    cleanup.disarm();
}

namespace detail {

std::filesystem::path makeTmpPath(
    const std::filesystem::path& dir, const std::string& filename) {
    return dir / (filename + ".tmp." + randomHexSuffix());
}

void writeTmpFileSync(const std::filesystem::path& tmpPath, const std::string& data) {
    const int fd = portableOpen(tmpPath);
    if (fd < 0) {
        throwErrno(tmpPath, "writeTmpFileSync: open");
    }
    if (!portableWriteFull(fd, data)) {
        const int writeErr = errno;
        portableClose(fd);
        errno = writeErr;
        throwErrno(tmpPath, "writeTmpFileSync: write");
    }
    if (!portableSync(fd)) {
        const int syncErr = errno;
        portableClose(fd);
        errno = syncErr;
        throwErrno(tmpPath, "writeTmpFileSync: fsync");
    }
    portableClose(fd);
}

}  // namespace detail
