#include "check.h"

#include <liblog/liblog.h>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static std::string readFile(const std::string& path)
{
	std::ifstream in(path);
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

int main()
{
	const std::chrono::system_clock::time_point ts{std::chrono::seconds{1609459200}};
	const std::string path = "/tmp/liblog_test_file.log";

	// ошибки init
	{
		liblog::FileWorker w;
		CHECK(w.ready() == false);
		CHECK(w.init("") == -1);
		CHECK(w.init("/nonexistent_dir_xyz/log.txt") == -1);
		CHECK(w.ready() == false);
		CHECK(w.log(liblog::LogLevels::Info, "x", ts) == -1);
	}

	// нормальный init и повторный
	{
		std::remove(path.c_str());
		liblog::FileWorker w;
		CHECK(w.init(path) == 0);
		CHECK(w.ready() == true);
		CHECK(w.init(path) == -1);
		CHECK(w.log(liblog::LogLevels::Error, "сообщение", ts) == 0);
	}

	// формат записи проверяем после разрушения воркера:
	// без flush данные доходят до диска только при закрытии файла
	CHECK(readFile(path) == "[ERROR] сообщение 2021-01-01T00:00:00Z\n");

	// все три уровня
	{
		std::remove(path.c_str());
		liblog::FileWorker w;
		w.init(path);
		CHECK(w.log(liblog::LogLevels::Error,   "a", ts) == 0);
		CHECK(w.log(liblog::LogLevels::Warning, "b", ts) == 0);
		CHECK(w.log(liblog::LogLevels::Info,    "c", ts) == 0);
	}
	CHECK(readFile(path) ==
		"[ERROR] a 2021-01-01T00:00:00Z\n"
		"[WARNING] b 2021-01-01T00:00:00Z\n"
		"[INFO] c 2021-01-01T00:00:00Z\n");

	std::remove(path.c_str());
	return g_failures == 0 ? 0 : 1;
}