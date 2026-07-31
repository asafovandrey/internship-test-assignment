#include "check.h"

#include <liblog/liblog.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

class Receiver
{
    int         fd_;
    std::string path_;

public:
    explicit Receiver(const std::string& path) : fd_(-1), path_(path)
    {
        std::remove(path_.c_str());

        fd_ = ::socket(AF_UNIX, SOCK_DGRAM, 0);
        if (fd_ < 0) return;

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, path_.c_str(), path_.size());

        if (::bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            ::close(fd_);
            fd_ = -1;
            return;
        }

        timeval tv{};
        tv.tv_sec = 1;
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    ~Receiver()
    {
        if (fd_ >= 0) ::close(fd_);
        std::remove(path_.c_str());
    }

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    bool ok() const { return fd_ >= 0; }

    std::string receive()
    {
        std::vector<char> buf(4096);
        const ssize_t n = ::recv(fd_, buf.data(), buf.size(), 0);
        if (n <= 0) return std::string();
        return std::string(buf.data(), static_cast<std::size_t>(n));
    }
};

int main()
{
    const std::chrono::system_clock::time_point ts{std::chrono::seconds{1609459200}};
    const std::string path = "/tmp/liblog_test.sock";

    {
        liblog::UdsWorker w;
        CHECK(w.ready() == false);
        CHECK(w.init("") == -1);
        CHECK(w.init(std::string(200, 'a')) == -1);
        CHECK(w.log(liblog::LogLevels::Info, "x", ts) == -1);
    }

    {
        liblog::UdsWorker w;
        CHECK(w.init("/tmp/liblog_nobody.sock") == -2);
        CHECK(w.ready() == false);
    }

    {
        Receiver rx(path);
        CHECK(rx.ok() == true);

        liblog::UdsWorker w;
        CHECK(w.init(path) == 0);
        CHECK(w.ready() == true);
        CHECK(w.init(path) == -1);

        CHECK(w.log(liblog::LogLevels::Error, "по сокету", ts) == 0);
        CHECK(rx.receive() == "[ERROR] по сокету 2021-01-01T00:00:00Z\n");

        CHECK(w.log(liblog::LogLevels::Warning, "раз", ts) == 0);
        CHECK(w.log(liblog::LogLevels::Info,    "два", ts) == 0);
        CHECK(rx.receive() == "[WARNING] раз 2021-01-01T00:00:00Z\n");
        CHECK(rx.receive() == "[INFO] два 2021-01-01T00:00:00Z\n");
    }

    {
        liblog::UdsWorker w;
        {
            Receiver rx(path);
            CHECK(w.init(path) == 0);
        }
        CHECK(w.log(liblog::LogLevels::Error, "в никуда", ts) == -2);
    }

    return g_failures == 0 ? 0 : 1;
}
