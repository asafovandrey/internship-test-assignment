#pragma once

#include <string>
#include <chrono>
#include <fstream>
#include <memory>

namespace liblog
{
	enum class LogLevels : unsigned char
	{
		Error, Warning, Info
	};

	class WriteWorkerInterface
	{
	public:
		virtual int log(LogLevels logLevel, const std::string& msg, std::chrono::system_clock::time_point timestamp) = 0;
		virtual ~WriteWorkerInterface() = default;

		WriteWorkerInterface(const WriteWorkerInterface&) = delete;
		WriteWorkerInterface& operator=(const WriteWorkerInterface&) = delete;

	protected:
		WriteWorkerInterface() = default;

		static std::string prepare_record(LogLevels logLevel, const std::string& msg, std::chrono::system_clock::time_point timestamp);
	};

	class FileWorker final : public WriteWorkerInterface
	{
		std::ofstream fout;
		bool inited = false;
	public:
		int init(const std::string& filename);
		bool ready();

		int log(LogLevels logLevel, const std::string& msg, std::chrono::system_clock::time_point timestamp) override;
	};

	class UdsWorker final : public WriteWorkerInterface
	{
		int fd_ = -1;

	public:
		UdsWorker() = default;
		~UdsWorker() override;

		int init(const std::string& socket_path);
		int log(LogLevels logLevel, const std::string& msg, std::chrono::system_clock::time_point timestamp) override;
		bool ready();
	};

	class Logger
	{		

		std::unique_ptr<WriteWorkerInterface> writeWorker_;
		LogLevels logLevel_;
		bool inited = false;

	public:
		int init(std::unique_ptr<WriteWorkerInterface> wwi, const LogLevels logLevel);
		bool ready();

		void changeLogLevel(const LogLevels logLevel);

		int log(LogLevels logLevel, const std::string& msg, std::chrono::system_clock::time_point timestamp);
	};
}
