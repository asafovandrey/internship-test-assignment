#include "liblog.h"

#include <ctime>
#include <iomanip>
#include <sstream>

std::string liblog::WriteWorkerInterface::prepare_record(LogLevels logLevel, const std::string& msg, std::chrono::system_clock::time_point timestamp)
{
    const std::time_t t = std::chrono::system_clock::to_time_t(timestamp);

    std::tm tmBuf{};
    ::gmtime_r(&t, &tmBuf);

    std::ostringstream ss;
    if (logLevel == LogLevels::Error) ss << "[ERROR] ";
    else if (logLevel == LogLevels::Warning) ss << "[WARNING] ";
    else if (logLevel == LogLevels::Info) ss << "[INFO] ";
    ss << msg << ' ' << std::put_time(&tmBuf, "%Y-%m-%dT%H:%M:%SZ") << '\n';

    return ss.str();
}
