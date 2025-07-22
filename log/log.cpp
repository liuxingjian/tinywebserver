#include "log.h"
#include <iostream>
#include <iomanip>
#include <sstream>

Logger::Logger() : running_(false), async_(true), max_lines_(5000), cur_lines_(0) {}

Logger::~Logger() {
    running_ = false;
    if (log_thread_ && log_thread_->joinable())
        log_thread_->join();
    if (log_file_.is_open())
        log_file_.close();
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::init(bool async, const std::string& filename, size_t max_lines) {
    async_ = async;
    log_filename_ = filename;
    max_lines_ = max_lines;
    cur_lines_ = 0;

    std::string full_name = log_filename_ + "_" + get_time().substr(0, 10) + ".log";
    log_file_.open(full_name, std::ios::app);

    if (async_) {
        log_queue_ = std::make_unique<BlockQueue<std::string>>();
        running_ = true;
        log_thread_ = std::make_unique<std::thread>(&Logger::async_write_thread, this);
    }
}

void Logger::write(LogLevel level, const std::string& msg) {
    std::stringstream ss;
    ss << "[" << get_time() << "] "
       << "[" << level_to_string(level) << "] "
       << msg << "\n";

    if (async_) {
        log_queue_->push(ss.str());
    } else {
        std::lock_guard<std::mutex> lock(file_mtx_);
        log_file_ << ss.str();
        cur_lines_++;
        if (cur_lines_ >= max_lines_) roll_file();
    }
}

void Logger::async_write_thread() {
    while (running_) {
        std::string log_msg;
        if (log_queue_->pop(log_msg)) {
            std::lock_guard<std::mutex> lock(file_mtx_);
            log_file_ << log_msg;
            cur_lines_++;
            if (cur_lines_ >= max_lines_) roll_file();
        }
    }
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(file_mtx_);
    log_file_.flush();
}

std::string Logger::get_time() {
    auto now = std::time(nullptr);
    std::tm t = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&t, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case INFO: return "INFO";
        case WARN: return "WARN";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::roll_file() {
    log_file_.close();
    std::string full_name = log_filename_ + "_" + get_time().substr(0, 10) + ".log";
    log_file_.open(full_name, std::ios::app);
    cur_lines_ = 0;
}
