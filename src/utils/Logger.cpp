#pragma warning(disable : 4996)
#include "Logger.hpp"

Logger* Logger::instance = nullptr;

Logger::Logger() {
    logFile.open("raytracer.log", std::ios::app);
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timePoint = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&timePoint));

    std::ostringstream ss;
    ss << buffer << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

Logger* Logger::getInstance() {
    if (instance == nullptr) {
        instance = new Logger();
    }
    return instance;
}

void Logger::log(const std::string& message) {
    if (!logFile.is_open()) return;

    std::lock_guard<std::mutex> lock(mtx);
    logFile << "[" << getCurrentTimestamp() << "] " << message << std::endl;
    logFile.flush();
}

void Logger::log(const std::wstring& message) {
    std::string narrowMsg(message.begin(), message.end());
    log(narrowMsg);
}

void Logger::cleanup() {
    delete instance;
    instance = nullptr;
}