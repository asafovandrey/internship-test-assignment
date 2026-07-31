#include "liblog.h"

int liblog::Logger::init(std::unique_ptr<WriteWorkerInterface> wwi, const LogLevels logLevel)
{
    if (inited)
    {
        return -1;
    }

    writeWorker_ = std::move(wwi);
    if (!writeWorker_)
    {
        return -1;
    }
    logLevel_ = logLevel;
    inited = true;
    return 0;
}

bool liblog::Logger::ready()
{
    return inited;
}

void liblog::Logger::changeLogLevel(const LogLevels logLevel)
{
    logLevel_ = logLevel;
}

int liblog::Logger::log(LogLevels logLevel, const std::string& msg, std::chrono::system_clock::time_point timestamp)
{
    if (!inited)
    {
        return -1;
    }
    if (logLevel > logLevel_)
    {
        return 1;
    }
    return writeWorker_->log(logLevel, msg, timestamp);
}
