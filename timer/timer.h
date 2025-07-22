#ifndef TIMER_H
#define TIMER_H

#include <queue>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <sys/timerfd.h>
#include <unistd.h>

class TimerManager {
public:
    using Clock = std::chrono::steady_clock;
    using Timestamp = Clock::time_point;

    TimerManager();

    void add_timer(int fd, int timeout_ms, std::function<void()> callback);
    void update_timer(int fd, int timeout_ms);
    void handle_expired();

    int get_timer_fd() const { return timer_fd_; }

private:
    struct TimerNode {
        int fd;
        Timestamp expire;
        std::function<void()> cb;

        bool operator>(const TimerNode& other) const {
            return expire > other.expire;
        }
    };

    void set_next_expire();

    std::priority_queue<TimerNode, std::vector<TimerNode>, std::greater<>> heap_;
    std::unordered_map<int, TimerNode> fd_map_;
    int timer_fd_;
};

#endif
