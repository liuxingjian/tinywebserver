#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class BlockQueue {
public:
    explicit BlockQueue(size_t max_size = 10000) : max_size_(max_size) {}

    void push(const T& item) {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_producer_.wait(lock, [this] { return queue_.size() < max_size_; });
        queue_.push(item);
        cond_consumer_.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_consumer_.wait(lock, [this] { return !queue_.empty(); });
        item = std::move(queue_.front());
        queue_.pop();
        cond_producer_.notify_one();
        return true;
    }

private:
    std::queue<T> queue_;
    size_t max_size_;
    std::mutex mtx_;
    std::condition_variable cond_producer_, cond_consumer_;
};

#endif
