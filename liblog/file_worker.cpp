#include "liblog.h"

int liblog::FileWorker::init(const std::string& filename)
{
    if (inited)
    {
        return -1;
    }
    if (filename.empty())
    {
        return -1;
    }
    fout.open(filename, std::ios::out);
    if (!fout.is_open())
    {
        return -1;
    }
    inited = true;
    return 0;
}

bool liblog::FileWorker::ready()
{
    return inited;
}

int liblog::FileWorker::log(LogLevels logLevel, const std::string& msg, std::chrono::system_clock::time_point timestamp)
{
    if (!fout.is_open())
    {
        return -1;
    }

    fout << prepare_record(logLevel, msg, timestamp);

    if (fout)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}
