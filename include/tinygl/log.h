#pragma once
#include <iostream>
#include <string>

namespace tinygl {

enum LogLevel { INFO, WARN, ERRR };

class Logger {
public:
    static void log(LogLevel level, const std::string& funcName, const std::string& msg) {
        switch (level) {
            case INFO: std::cout << "[INFO]  "; break;
            case WARN: std::cout << "[WARN]  "; break;
            case ERRR: std::cerr << "[ERROR] "; break;
        }
        std::cout << "[" << funcName << "] " << msg << std::endl;
    }
};

#define LOG_INFO(msg) Logger::log(INFO, __FUNCTION__, msg)
#define LOG_WARN(msg) Logger::log(WARN, __FUNCTION__, msg)
#define LOG_ERROR(msg) Logger::log(ERRR, __FUNCTION__, msg)

}