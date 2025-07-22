#include "threadpool.h"
//初始化线程池，创建并启动指定数量的工作线程。

ThreadPool::ThreadPool(size_t thread_count):stop(false){
    for(size_t i=0;i<thread_count;i++){
        //循环创建thread_count个工作线程，每个线程执行worker()方法。
        workers.emplace_back([this](){this->worker();});
    }
}


ThreadPool::~ThreadPool() {
    stop = true;               // 标记线程池停止
    cv.notify_all();           // 唤醒所有等待的线程
    for (auto &thread : workers) {
        if (thread.joinable())
            thread.join();     // 等待所有线程完成当前任务
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mtx);  // 加锁保护任务队列
        tasks.emplace(std::move(task));          // 将任务移入队列
    }                                              // 锁在此处释放
    cv.notify_one();                             // 唤醒一个等待的线程
}
void ThreadPool::worker() {
    while (!stop) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mtx);
            // 等待任务或停止信号
            cv.wait(lock, [this]() { return stop || !tasks.empty(); });
            
            // 如果线程池停止且队列为空，直接返回
            if (stop && tasks.empty()) return;
            
            task = std::move(tasks.front());  // 取出任务
            tasks.pop();                      // 从队列移除
        }  // 锁在此处释放，允许其他线程同时处理队列
        
        task();  // 执行任务（在锁外执行，避免阻塞队列）
    }
}