//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace sudoku_hpc {

class DebugLogger {
public:
    DebugLogger() {
        try {
            const auto now = std::chrono::system_clock::now();
            const auto t = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif
            std::ostringstream name;
            name << "sudoku_debug_"
                 << std::put_time(&tm, "%Y%m%d_%H%M%S")
                 << ".log";
            const std::filesystem::path debug_dir = std::filesystem::current_path() / "Debugs";
            std::filesystem::create_directories(debug_dir);
            path_ = (debug_dir / name.str()).string();
            stream_.open(path_, std::ios::out | std::ios::app);
        } catch (...) {
            try {
                const std::filesystem::path debug_dir = std::filesystem::current_path() / "Debugs";
                std::filesystem::create_directories(debug_dir);
                path_ = (debug_dir / "sudoku_debug.log").string();
            } catch (...) {
                path_ = "Debugs\\sudoku_debug.log";
            }
            stream_.open(path_, std::ios::out | std::ios::app);
        }
    }

    const std::string& path() const {
        return path_;
    }

    void write(const char* level, const std::string& scope, const std::string& msg) {
        std::lock_guard<std::mutex> lock(mu_);
        if (!stream_) {
            return;
        }
        const auto now = std::chrono::system_clock::now();
        const auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        stream_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
                << " [" << level << "] "
                << "[tid=" << std::this_thread::get_id() << "] "
                << "(" << scope << ") "
                << msg
                << "\n";
        stream_.flush();
    }

private:
    std::string path_;
    std::ofstream stream_;
    std::mutex mu_;
};

inline DebugLogger& debug_logger() {
    static DebugLogger logger;
    return logger;
}

inline void log_info(const std::string& scope, const std::string& msg) {
    debug_logger().write("INFO", scope, msg);
}

inline void log_warn(const std::string& scope, const std::string& msg) {
    debug_logger().write("WARN", scope, msg);
}

inline void log_error(const std::string& scope, const std::string& msg) {
    debug_logger().write("ERROR", scope, msg);
}

} // namespace sudoku_hpc
