#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <memory>
#include <vector>
#include <string>
#include <iostream>

namespace tinygl {

class LogSystem {
public:
    static std::shared_ptr<spdlog::logger> GetLogger() {
        static std::shared_ptr<spdlog::logger> logger = []() -> std::shared_ptr<spdlog::logger> {
            try {
                // Console sink
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_level(spdlog::level::info);
                console_sink->set_pattern("[%^%l%$]  %v"); 

                // File sink (tinygl.log in execution directory)
                // truncate = true (overwrite log file on each run)
                auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("tinygl.log", true);
                file_sink->set_level(spdlog::level::trace);
                file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

                std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
                
                auto new_logger = std::make_shared<spdlog::logger>("tinygl_logger", sinks.begin(), sinks.end());
                new_logger->set_level(spdlog::level::trace);
                new_logger->flush_on(spdlog::level::info);
                
                return new_logger;
            } catch (const spdlog::spdlog_ex& ex) {
                std::cerr << "Log initialization failed: " << ex.what() << std::endl;
                // Return a default logger to avoid crashes
                return spdlog::stdout_color_mt("console"); 
            }
        }();
        
        return logger;
    }
};

// Updated macros to use spdlog
// We keep the signature LOG_XXX(msg) and format the function name into it.
// Note: msg is expected to be a string or something printable.
#define LOG_INFO(msg)  tinygl::LogSystem::GetLogger()->info("[{}] {}", __FUNCTION__, msg)
#define LOG_WARN(msg)  tinygl::LogSystem::GetLogger()->warn("[{}] {}", __FUNCTION__, msg)
#define LOG_ERROR(msg) tinygl::LogSystem::GetLogger()->error("[{}] {}", __FUNCTION__, msg)

}
