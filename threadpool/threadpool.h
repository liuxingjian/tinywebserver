#ifndef THREADPOOL_H    // 防止头文件被重复包含
#define THREADPOOL_H    // 定义头文件保护宏

#include <vector>       // 存储工作线程
#include <queue>        // 任务队列
#include <thread>       // C++11线程库
#include <mutex>        // 互斥锁，保护共享资源
#include <condition_variable> // 条件变量，用于线程同步
#include <functional>   // 函数对象，存储任务
#include <atomic>       // 原子操作，保证线程安全

class ThreadPool {
public:
    // 构造函数：指定线程池中的线程数量
    explicit ThreadPool(size_t thread_count);
    
    // 析构函数：清理资源并停止所有线程
    ~ThreadPool();

    // 向任务队列添加任务
    void enqueue(std::function<void()> task);

private:
    // 工作线程的主函数
    void worker();

    // 成员变量
    std::vector<std::thread> workers;        // 存储工作线程
    std::queue<std::function<void()>> tasks; // 任务队列

    std::mutex mtx;                // 保护任务队列的互斥锁
    std::condition_variable cv;    // 条件变量，用于唤醒等待的线程
    std::atomic<bool> stop;        // 原子标志，指示线程池是否停止
};
#endif // THREADPOOL_H
