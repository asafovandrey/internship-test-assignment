#include "check.h"

#include <liblog/liblog.h>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

int main()
{
	const std::chrono::system_clock::time_point ts{std::chrono::seconds{1609459200}};
	const std::string path = "/tmp/liblog_test_logger.log";

	{
		liblog::Logger lg;
		CHECK(lg.ready() == false);
		CHECK(lg.log(liblog::LogLevels::Error, "x", ts) == -1);
	}

	{
		liblog::Logger lg;
		CHECK(lg.init(nullptr, liblog::LogLevels::Info) == -1);
		CHECK(lg.ready() == false);
	}

	{
		std::remove(path.c_str());
		std::unique_ptr<liblog::FileWorker> fw(new liblog::FileWorker());
		CHECK(fw->init(path) == 0);

		liblog::Logger lg;
		CHECK(lg.init(std::unique_ptr<liblog::WriteWorkerInterface>(fw.release()),
		              liblog::LogLevels::Warning) == 0);
		CHECK(lg.ready() == true);
		CHECK(lg.init(nullptr, liblog::LogLevels::Info) == -1);

		CHECK(lg.log(liblog::LogLevels::Error, "e", ts) == 0);
		CHECK(lg.log(liblog::LogLevels::Warning, "w", ts) == 0);
		CHECK(lg.log(liblog::LogLevels::Info, "i", ts) == 1);

		lg.changeLogLevel(liblog::LogLevels::Info);
		CHECK(lg.log(liblog::LogLevels::Info, "i2", ts) == 0);

		lg.changeLogLevel(liblog::LogLevels::Error);
		CHECK(lg.log(liblog::LogLevels::Warning, "w2", ts) == 1);
	}
	std::remove(path.c_str());

	return g_failures == 0 ? 0 : 1;
}
