#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstring>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace
{
	volatile std::sig_atomic_t g_stop = 0;

	void onSignal(int)
	{
		g_stop = 1;
	}

	bool parseRecord(const std::string& line, std::string& levelOut, std::string& textOut, std::time_t& timeOut)
	{
		if (line.empty() || line[0] != '[')
		{
			return false;
		}

		const std::size_t close = line.find(']');
		if (close == std::string::npos)
		{
			return false;
		}

		levelOut = line.substr(1, close - 1);

		std::size_t start = close + 1;
		while (start < line.size() && line[start] == ' ')
		{
			++start;
		}

		std::string rest = line.substr(start);
		while (!rest.empty() && (rest.back() == '\n' || rest.back() == '\r'))
		{
			rest.pop_back();
		}

		const std::size_t sep = rest.rfind(' ');
		if (sep == std::string::npos)
		{
			return false;
		}

		textOut = rest.substr(0, sep);
		const std::string stamp = rest.substr(sep + 1);

		std::tm tm{};
		std::istringstream ss(stamp);
		ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
		if (ss.fail())
		{
			return false;
		}

		timeOut = ::timegm(&tm);
		return true;
	}

	void printStats(unsigned long total, unsigned long errors, unsigned long warnings, unsigned long infos, std::size_t lastHour, std::size_t minLen, std::size_t maxLen, unsigned long long sumLen)
	{
		std::cout << "\n    статистика    \n";
		std::cout << "сообщений всего:      " << total << '\n';
		std::cout << "  ERROR:              " << errors << '\n';
		std::cout << "  WARNING:            " << warnings << '\n';
		std::cout << "  INFO:               " << infos << '\n';
		std::cout << "за последний час:     " << lastHour << '\n';

		if (total == 0)
		{
			std::cout << "длина сообщений:      нет данных\n";
		}
		else
		{
			std::cout << "длина сообщений: мин " << minLen << ", макс " << maxLen << ", средняя " << std::fixed << std::setprecision(1) << (static_cast<double>(sumLen) / static_cast<double>(total)) << '\n';
		}
		std::cout << "------------------\n\n";
	}

	void printUsage(const char* program)
	{
		std::cerr << "Использование: " << program << " <путь к сокету> <N> <T>\n  N - выдавать статистику после каждого N-го сообщения\n  T - выдавать статистику по таймауту T секунд, если она изменилась\nПример: " << program << " /tmp/liblog.sock 10 5\n";
	}
}

int main(int argc, char** argv)
{
	if (argc != 4)
	{
		printUsage(argc > 0 ? argv[0] : "stats_app");
		return 1;
	}

	const std::string socketPath = argv[1];

	long n = 0;
	long t = 0;
	try
	{
		n = std::stol(argv[2]);
		t = std::stol(argv[3]);
	}
	catch (...)
	{
		std::cerr << "N и T должны быть числами\n";
		printUsage(argv[0]);
		return 1;
	}

	if (n <= 0 || t <= 0)
	{
		std::cerr << "N и T должны быть больше нуля\n";
		return 1;
	}

	if (socketPath.empty() || socketPath.size() >= sizeof(sockaddr_un::sun_path))
	{
		std::cerr << "Некорректный путь к сокету\n";
		return 1;
	}

	std::signal(SIGINT, onSignal);
	std::signal(SIGTERM, onSignal);

	::unlink(socketPath.c_str());

	const int fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0)
	{
		std::cerr << "Не удалось создать сокет\n";
		return 1;
	}

	sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	std::memcpy(addr.sun_path, socketPath.c_str(), socketPath.size());

	if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0)
	{
		std::cerr << "Не удалось привязать сокет: " << socketPath << '\n';
		::close(fd);
		return 1;
	}

	timeval tv{};
	tv.tv_usec = 200000;
	::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	std::cout << "Слушаю " << socketPath << ", N=" << n << ", T=" << t << ". Ctrl+C — выход.\n";

	unsigned long total = 0;
	unsigned long errors = 0;
	unsigned long warnings = 0;
	unsigned long infos = 0;
	std::size_t minLen = 0;
	std::size_t maxLen = 0;
	unsigned long long sumLen = 0;
	unsigned long totalAtLastPrint = 0;

	std::deque<std::time_t> recent;
	std::vector<char> buffer(65536);
	auto lastPrint = std::chrono::steady_clock::now();

	while (g_stop == 0)
	{
		const ssize_t received = ::recv(fd, buffer.data(), buffer.size(), 0);

		if (received > 0)
		{
			const std::string line(buffer.data(), static_cast<std::size_t>(received));

			std::string level;
			std::string text;
			std::time_t stamp = 0;

			if (!parseRecord(line, level, text, stamp))
			{
				std::cerr << "Некорректная запись, пропущена\n";
				continue;
			}

			std::cout << "[" << level << "] " << text << '\n';

			++total;
			if (level == "ERROR")
			{
				++errors;
			}
			else if (level == "WARNING")
			{
				++warnings;
			}
			else if (level == "INFO")
			{
				++infos;
			}

			const std::size_t length = text.size();
			if (total == 1 || length < minLen)
			{
				minLen = length;
			}
			if (length > maxLen)
			{
				maxLen = length;
			}
			sumLen += length;

			recent.push_back(stamp);
		}

		const std::time_t now = std::time(nullptr);
		while (!recent.empty() && now - recent.front() > 3600)
		{
			recent.pop_front();
		}

		if (received > 0 && total % static_cast<unsigned long>(n) == 0)
		{
			printStats(total, errors, warnings, infos, recent.size(), minLen, maxLen, sumLen);
			totalAtLastPrint = total;
			lastPrint = std::chrono::steady_clock::now();
			continue;
		}

		const auto elapsed = std::chrono::steady_clock::now() - lastPrint;
		if (elapsed >= std::chrono::seconds(t))
		{
			if (total != totalAtLastPrint)
			{
				printStats(total, errors, warnings, infos, recent.size(), minLen, maxLen, sumLen);
				totalAtLastPrint = total;
			}
			lastPrint = std::chrono::steady_clock::now();
		}
	}

	std::cout << "\nЗавершение\n";
	printStats(total, errors, warnings, infos, recent.size(), minLen, maxLen, sumLen);

	::close(fd);
	::unlink(socketPath.c_str());
	return 0;
}
