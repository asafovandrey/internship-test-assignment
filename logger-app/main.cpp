#include <liblog/liblog.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <tuple>

namespace
{
    using Record = std::tuple<liblog::LogLevels, std::string, std::chrono::system_clock::time_point>;

    const char* levelName(liblog::LogLevels level)
    {
        switch (level)
        {
        case liblog::LogLevels::Error:
        {
            return "Error";
        }
        case liblog::LogLevels::Warning:
        {
            return "Warning";
        }
        case liblog::LogLevels::Info:
        {
            return "Info";
        }
        }
        return "Unknown";
    }

    bool parseLevel(const std::string& text, liblog::LogLevels& out)
    {
        if (text == "0" || text == "error")
        {
            out = liblog::LogLevels::Error;
            return true;
        }
        if (text == "1" || text == "warning")
        {
            out = liblog::LogLevels::Warning;
            return true;
        }
        if (text == "2" || text == "info")
        {
            out = liblog::LogLevels::Info;
            return true;
        }
        return false;
    }

    void cli_procedure(std::mutex& mutex, std::queue<Record>& queue, std::atomic<bool>& running, std::atomic<bool>& has_data, liblog::LogLevels defaultLevel)
    {
        std::cout << "Многопоточное приложение для лога событий\n";

        while (running.load())
        {
            std::cout << "\nДля ввода уровня важности введите:\n  0 - Error\n  1 - Warning\n  2 - Info\n  3 - По умолчанию (" << levelName(defaultLevel) << ")\n  4 - Выход из приложения\n";

            std::string answer;
            if (!std::getline(std::cin, answer))
            {
                std::cout << "Конец ввода\n";
                break;
            }

            if (answer == "4")
            {
                break;
            }

            liblog::LogLevels level;
            if (answer == "3")
            {
                level = defaultLevel;
            }
            else if (!parseLevel(answer, level))
            {
                std::cout << "Неверный формат!\n";
                continue;
            }

            std::cout << "Введите сообщение: ";
            std::string msg;
            if (!std::getline(std::cin, msg))
            {
                std::cout << "Конец ввода\n";
                break;
            }

            if (msg.find_first_not_of(" \t") == std::string::npos)
            {
                std::cout << "Пустое сообщение, пропущено\n";
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                queue.push(Record{level, std::move(msg), std::chrono::system_clock::now()});
                has_data.store(true);
            }
        }

        running.store(false);
    }

    void logger_procedure(liblog::Logger& logger, std::mutex& mutex, std::queue<Record>& queue, std::atomic<bool>& running, std::atomic<bool>& has_data)
    {
        while (running.load() || has_data.load())
        {
            Record record;
            bool got = false;

            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!queue.empty())
                {
                    record = std::move(queue.front());
                    queue.pop();
                    has_data.store(!queue.empty());
                    got = true;
                }
            }

            if (!got)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            const auto& [level, msg, timestamp] = record;
            if (logger.log(level, msg, timestamp) < 0)
            {
                std::cerr << "Ошибка записи в журнал\n";
            }
        }
    }

    void printUsage(const char* program)
    {
        std::cerr << "Использование: " << program << " <файл журнала> <уровень по умолчанию>\n  уровень: 0 | error, 1 | warning, 2 | info\nПример: " << program << " app.log warning\n";
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printUsage(argc > 0 ? argv[0] : "logger_app");
        return 1;
    }

    const std::string logFile = argv[1];

    liblog::LogLevels defaultLevel;
    if (!parseLevel(argv[2], defaultLevel))
    {
        std::cerr << "Неизвестный уровень важности: " << argv[2] << '\n';
        printUsage(argv[0]);
        return 1;
    }

    std::unique_ptr<liblog::FileWorker> worker(new liblog::FileWorker());
    if (worker->init(logFile) != 0)
    {
        std::cerr << "Не удалось открыть файл журнала: " << logFile << '\n';
        return 1;
    }

    liblog::Logger logger;
    if (logger.init(std::unique_ptr<liblog::WriteWorkerInterface>(worker.release()), defaultLevel) != 0)
    {
        std::cerr << "Не удалось инициализировать журнал\n";
        return 1;
    }

    std::mutex mutex;
    std::queue<Record> queue;
    std::atomic<bool> running{true};
    std::atomic<bool> has_data{false};

    std::thread writer(logger_procedure, std::ref(logger), std::ref(mutex), std::ref(queue), std::ref(running), std::ref(has_data));

    cli_procedure(mutex, queue, running, has_data, defaultLevel);

    writer.join();

    std::cout << "Завершение\n";
    return 0;
}
