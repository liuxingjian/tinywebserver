/**
 * @file threadpool.h
 * @brief 定义了一个简单的线程池，用于并发管理和执行任务。
 *
 * ThreadPool 类提供了一个单例线程池实现，允许添加任务并由固定数量的工作线程执行。
 * 任务存储在队列中，由分离的线程处理，当队列为空时线程会等待新任务。
 *
 * 主要特性：
 * - 通过 instance() 获取单例
 * - 可配置线程数量和最大请求队列长度
 * - 使用 addTask() 线程安全地添加任务
 * - 自动管理线程并支持优雅关闭
 * - 使用 std::function<void()> 灵活表示任务
 *
 * 用法：
 * - 调用 ThreadPool::instance()->init(threadNum, maxRequests) 初始化线程池。
 * - 使用 ThreadPool::instance()->addTask(task) 添加任务。
 * - 线程池自动管理线程和任务执行。
 *
 * 线程安全通过自定义 locker 和条件变量类实现。
 */
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "../lock/locker.h"   // 引入自定义的锁/条件/信号量封装（mtx, cond 等）
#include <queue>              // 使用 std::queue 存放任务
#include <thread>             // 使用 std::thread 创建工作线程
#include <functional>         // 使用 std::function 表示任务回调
#include <memory>             // 使用智能指针（若需要）
#include <assert.h>           // 使用 assert 进行断言检查

using namespace std;         // 使用 std 命名空间，简化类型书写

class ThreadPool
{
public:
    static ThreadPool* instance()    // 获取线程池单例
    {
        static ThreadPool threadpool; // 局部静态对象，C++11 起线程安全
        return &threadpool;          // 返回单例指针
    }

    /* 回调函数：线程循环从任务队列中取任务执行 */
    static void callback(ThreadPool* pool)
    {
        while(true)
        {
            pool->mtxPool.lock();                             // 加锁保护任务队列访问
            while (pool->tasks.empty() && !pool->shutdown)   // 当任务为空且未关闭时
            {
                pool->condNotEmpty.wait(pool->mtxPool.get()); // 等待条件变量，释放锁并阻塞
            }

            if (pool->shutdown)                              // 如果线程池被标记为关闭
            {
                pool->mtxPool.unlock();                      // 解锁
                break;                                       // 退出循环，线程结束
            }
            
            // 从队列取出任务并使用 move 避免拷贝
            auto task = move(pool->tasks.front());           // 取得队首任务，转换为右值
            pool->tasks.pop();                               // 弹出已取得的任务
            pool->mtxPool.unlock();                          // 解锁，允许其他线程访问队列
            task();                                          // 执行任务（函数对象已绑定参数）
        }
    }
    
    void init (int threadNum = 8, int maxRequests = 10000) // 初始化线程池：线程数与最大队列长度
    {
        this->threadNum = threadNum;            // 记录线程数
        this->maxRequests = maxRequests;        // 记录最大请求数
        assert(threadNum > 0);                  // 断言线程数必须大于0
        // 初始化时创建所有工作线程，线程无任务时阻塞等待
        for (int i = 0; i < threadNum; i ++)
        {
            // 创建线程并传入回调，线程分离（detach），主线程无需 join 回收
            thread(callback, this).detach();
        }
    }

    /* 添加任务到线程池任务队列 */
    // 传入已经用 bind/lamdba 打包好的函数对象，F&& 表示通用引用（可接收右值）
    template<typename F>
    void addTask(F&& task)
    {
        mtxPool.lock();                               // 加锁保护任务队列
        if ((int)tasks.size() < maxRequests)          // 若队列未满
        {
            // 完美转发任务到队列（保持右值性质以避免不必要拷贝）
            tasks.emplace(forward<F>(task));
            condNotEmpty.signal();                    // 通知等待线程（唤醒一个）
        }
        mtxPool.unlock();                             // 解锁
    }

private:
    mtx mtxPool;                      // 自定义互斥锁封装，用于保护任务队列
    cond condNotEmpty;                // 自定义条件变量封装，表示“非空”信号
    int threadNum;                    // 工作线程数量
    int maxRequests;                  // 最大请求/任务队列容量
    bool shutdown;                    // 线程池是否关闭标志
    queue<function<void()>> tasks;    // 任务队列，存放可调用的无参无返回任务

    ThreadPool(){}                     // 私有构造，单例模式
    ~ThreadPool()
    {
        mtxPool.lock();                // 加锁准备关闭线程池
        shutdown = true;               // 设置关闭标志，诱导工作线程退出循环
        mtxPool.unlock();              // 解锁
        condNotEmpty.broadcast();      // 唤醒所有等待线程，促使它们检查 shutdown 并退出
    }
};

#endif