#ifndef LOG_H
#define LOG_H

#include <string>
#include <thread>
#include <atomic>
#include <fstream>
#include <memory>
#include <ctime>
#include "block_queue.h"

enum LogLevel { INFO, WARN, ERROR };

class Logger {
public:
    static Logger& instance();

    void init(bool async = true, const std::string& filename = "server_log", size_t max_lines = 5000);
    void write(LogLevel level, const std::string& msg);
    void flush();

private:
    Logger();
    ~Logger();

    void async_write_thread();

    std::atomic<bool> running_;
    bool async_;
    size_t max_lines_;
    size_t cur_lines_;
    std::string log_filename_;
    std::ofstream log_file_;
    std::unique_ptr<BlockQueue<std::string>> log_queue_;
    std::unique_ptr<std::thread> log_thread_;
    std::mutex file_mtx_;
    std::string get_time();
    std::string level_to_string(LogLevel level);
    void roll_file();
};

#define LOG_INFO(msg) Logger::instance().write(INFO, msg)
#define LOG_WARN(msg) Logger::instance().write(WARN, msg)
#define LOG_ERROR(msg) Logger::instance().write(ERROR, msg)

#endif
