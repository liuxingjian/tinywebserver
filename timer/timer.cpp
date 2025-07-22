#include "timer.h"
#include <cstring>
#include <sys/epoll.h>
#include <iostream>

TimerManager::TimerManager() {
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer_fd_ < 0) {
        perror("timerfd_create");
        exit(1);
    }
}

void TimerManager::add_timer(int fd, int timeout_ms, std::function<void()> cb) {
    Timestamp expire = Clock::now() + std::chrono::milliseconds(timeout_ms);
    TimerNode node = {fd, expire, cb};
    heap_.push(node);
    fd_map_[fd] = node;
    set_next_expire();
}

void TimerManager::update_timer(int fd, int timeout_ms) {
    // 简化实现：直接重新插入
    add_timer(fd, timeout_ms, fd_map_[fd].cb);
}

void TimerManager::set_next_expire() {
    if (heap_.empty()) return;

    auto next = heap_.top().expire;
    auto now = Clock::now();
    int diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(next - now).count();
    if (diff_ms < 0) diff_ms = 0;

    struct itimerspec new_value{};
    new_value.it_value.tv_sec = diff_ms / 1000;
    new_value.it_value.tv_nsec = (diff_ms % 1000) * 1'000'000;
    timerfd_settime(timer_fd_, 0, &new_value, nullptr);
}

void TimerManager::handle_expired() {
    uint64_t exp;
    read(timer_fd_, &exp, sizeof(exp));  // 清除 timerfd 可读状态

    auto now = Clock::now();
    while (!heap_.empty()) {
        const auto& node = heap_.top();
        if (node.expire > now) break;

        if (fd_map_.count(node.fd)) {
            fd_map_.erase(node.fd);
            node.cb();  // 回调，关闭连接
        }
        heap_.pop();
    }

    set_next_expire();
}
