#pragma once
#include <fstream>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <ctime>

class Logger {
private:
    std::ofstream logFile;
    std::mutex mtx;
    static Logger* instance;

    Logger();
    ~Logger();

    std::string getCurrentTimestamp();

public:
    static Logger* getInstance();
    void log(const std::string& message);
    void log(const std::wstring& message);

    template<typename T>
    void log(const T& value) {
        std::ostringstream ss;
        ss << value;
        log(ss.str());
    }

    static void cleanup();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

#define LOG(msg) Logger::getInstance()->log(msg)