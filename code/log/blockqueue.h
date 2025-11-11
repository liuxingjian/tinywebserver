
/**
 * @file blockqueue.h
 * @brief 基于生产者-消费者模型实现的线程安全阻塞队列。
 *
 * 该类模板提供一个阻塞队列，可作为日志缓冲区，支持多线程并发访问。
 * 使用互斥锁和条件变量实现同步，保证线程安全。
 *
 * @tparam T 队列中存储的元素类型。
 *
 * 主要特性：
 * - 线程安全的 push 和 pop 操作。
 * - 当队列满时生产者阻塞，当队列空时消费者阻塞。
 * - pop 操作支持超时等待。
 * - 提供查询队列状态的方法（empty, full, size, capacity）。
 * - 支持清空和关闭队列。
 *
 * 使用方法：
 * - 生产者线程调用 push() 向队列添加元素。
 * - 消费者线程调用 pop() 或 pop(timeout) 获取队列元素。
 * - 调用 close() 后，所有等待线程被唤醒，队列关闭，后续操作失效。
 *
 * 注意事项：
 * - 所有队列操作均受互斥锁保护。
 * - 条件变量用于生产者和消费者之间的状态通知。
 */
/* 阻塞队列
阻塞队列作为日志缓冲区，内部封装了生产者-消费者模型
生产者：向队列尾部插入日志信息的线程
消费者：从队列头部取出日志信息并处理的线程

因为涉及到多线程读写，使用互斥锁实现对队列的互斥访问
同时用两个条件信号实现生产者-消费者模型

需要注意的点是插入和删除时，条件变量需要配合互斥锁
先上锁，然后在while循环内检查条件变量
当插入一个日志时，唤醒一个消费者线程
当处理一个日志时，唤醒一个生产者线程
*/








#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>                // 互斥锁，保证线程安全
#include <queue>                // 标准队列容器
#include <condition_variable>   // 条件变量，用于线程同步
#include <sys/time.h>           // 时间相关

// 阻塞队列模板类定义
template<class T>
class BlockQueue {
public:
    explicit BlockQueue(size_t MaxCapacity = 1000); // 构造函数，设置队列最大容量

    ~BlockQueue();                                  // 析构函数

    void clear();                                   // 清空队列

    bool empty();                                   // 判断队列是否为空

    bool full();                                    // 判断队列是否已满

    void close();                                   // 关闭队列，唤醒所有等待线程

    size_t size();                                  // 获取队列当前元素数量

    size_t capacity();                              // 获取队列最大容量

    T front();                                      // 获取队头元素

    T back();                                       // 获取队尾元素

    void push(const T &item);                       // 向队列尾部插入元素（生产者）

    bool pop(T &item);                              // 从队列头部取出元素（消费者），阻塞等待

    bool pop(T &item, int timeout);                 // 带超时的pop操作

    void flush();                                   // 唤醒一个消费者线程

private:
    std::queue<T> que;                             // 队列容器

    size_t capacity_;                              // 队列最大容量

    std::mutex mtx;                               // 互斥锁

    bool isClose;                                 // 队列关闭标志

    std::condition_variable condConsumer;         // 消费者条件变量

    std::condition_variable condProducer;         // 生产者条件变量
};


// 构造函数，初始化最大容量和关闭标志
template<class T>
BlockQueue<T>::BlockQueue(size_t maxCapacity) :capacity_(maxCapacity) {
    assert(maxCapacity > 0);
    isClose = false;
}

// 析构函数，调用close关闭队列
template<class T>
BlockQueue<T>::~BlockQueue() {
    close();
};

// 关闭队列，清空内容并唤醒所有等待线程
template<class T>
void BlockQueue<T>::close() {
    {   
        std::lock_guard<std::mutex> locker(mtx);
        // queue不支持clear，重新赋值一个空队列
        que = std::queue<T>();
        isClose = true;
    }
    condProducer.notify_all(); // 唤醒所有生产者
    condConsumer.notify_all(); // 唤醒所有消费者
};

// 唤醒一个消费者线程
template<class T>
void BlockQueue<T>::flush() {
    condConsumer.notify_one();
};

// 清空队列
template<class T>
void BlockQueue<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx);
    que.clear();
}

// 获取队头元素
template<class T>
T BlockQueue<T>::front() {
    std::lock_guard<std::mutex> locker(mtx);
    return que.front();
}

// 获取队尾元素
template<class T>
T BlockQueue<T>::back() {
    std::lock_guard<std::mutex> locker(mtx);
    return que.back();
}

// 获取队列当前元素数量
template<class T>
size_t BlockQueue<T>::size() {
    std::lock_guard<std::mutex> locker(mtx);
    return que.size();
}

// 获取队列最大容量
template<class T>
size_t BlockQueue<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx);
    return capacity_;
}

// 插入元素，队列满时阻塞等待
template<class T>
void BlockQueue<T>::push(const T &item) {
    std::unique_lock<std::mutex> locker(mtx);
    while(que.size() >= capacity_) {
        condProducer.wait(locker); // 队列满，等待生产者条件变量
    }
    que.push(item);                // 插入元素
    condConsumer.notify_one();     // 唤醒一个消费者线程
}

// 判断队列是否为空
template<class T>
bool BlockQueue<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx);
    return que.empty();
}

// 判断队列是否已满
template<class T>
bool BlockQueue<T>::full(){
    std::lock_guard<std::mutex> locker(mtx);
    return que.size() >= capacity_;
}

// 取出元素，队列空时阻塞等待
template<class T>
bool BlockQueue<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx);
    while(que.empty()){
        condConsumer.wait(locker); // 队列空，等待消费者条件变量
        if(isClose){               // 队列关闭则返回false
            return false;
        }
    }
    item = que.front();            // 取队头元素
    que.pop();                     // 删除队头元素
    condProducer.notify_one();     // 唤醒一个生产者线程
    return true;
}

// 带超时的pop操作
template<class T>
bool BlockQueue<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx);
    while(que.empty()){
        // 判断等待时间是否超过timeout
        if(condConsumer.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){
            return false;
        }
        if(isClose){
            return false;
        }
    }
    item = que.front();            // 取队头元素
    que.pop();                     // 删除队头元素
    condProducer.notify_one();     // 唤醒一个生产者线程
    return true;
}

#endif // BLOCKQUEUE_H