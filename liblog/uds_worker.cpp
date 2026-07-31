#include "liblog.h"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

liblog::UdsWorker::~UdsWorker()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

int liblog::UdsWorker::init(const std::string& socketPath)
{
    if (fd_ >= 0)
    {
        return -1;
    }
    if (socketPath.empty())
    {
        return -1;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    if (socketPath.size() >= sizeof(addr.sun_path))
    {
        return -1;
    }

    std::memcpy(addr.sun_path, socketPath.c_str(), socketPath.size());

    const int fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        const int err = errno;
        ::close(fd);
        if (err == ENOENT || err == ECONNREFUSED)
        {
            return -2;
        }
        return -1;
    }

    fd_ = fd;
    return 0;
}

int liblog::UdsWorker::log(LogLevels logLevel, const std::string& msg, std::chrono::system_clock::time_point timestamp)
{
    if (fd_ < 0)
    {
        return -1;
    }

    const std::string record = prepare_record(logLevel, msg, timestamp);
    const ssize_t sent = ::send(fd_, record.data(), record.size(), MSG_NOSIGNAL);

    if (sent < 0)
    {
        if (errno == ECONNREFUSED)
        {
            return -2;
        }
        return -1;
    }

    if (static_cast<std::size_t>(sent) != record.size())
    {
        return -1;
    }

    return 0;
}

bool liblog::UdsWorker::ready()
{
    return fd_ >= 0;
}
