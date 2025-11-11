#ifndef LOCKER_H
#define LOCKER_H
#include <exception>    // 引入异常支持，用于抛出 std::exception
#include <pthread.h>    // POSIX 线程/互斥/条件变量接口
#include <semaphore.h>  // POSIX 信号量接口
/**
 * @file locker.h
 * @brief 提供 POSIX 同步原语（信号量、互斥锁、条件变量）的包装类。
 *
 * 类说明：
 * - sem：POSIX 信号量（sem_t）的包装类，提供初始化、销毁、等待和释放操作。
 * - mtx：POSIX 互斥锁（pthread_mutex_t）的包装类，提供初始化、销毁、加锁、解锁及获取底层互斥锁指针操作。
 * - cond：POSIX 条件变量（pthread_cond_t）的包装类，提供初始化、销毁、等待、定时等待、信号和广播操作。
 *
 * 所有类在初始化失败时抛出 std::exception 异常。
 */
class sem
{
public:
    sem()
    {
        // 初始化信号量，初始值为0；失败则抛出异常
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            throw std::exception();
        }
    }
    sem(int num)
    {
        // 初始化信号量，初始值为 num；失败则抛出异常
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    }
    ~sem()
    {
        // 销毁信号量，释放内核资源
        sem_destroy(&m_sem);
    }
    bool wait()
    {
        // P 操作（等待/减），成功返回 true
        return sem_wait(&m_sem) == 0;
    }
    bool post()
    {
        // V 操作（释放/增），成功返回 true
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem; // 底层 POSIX 信号量对象
};


class mtx
{
public:
    mtx()
    {
        // 初始化互斥锁，失败则抛出异常
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
    }
    ~mtx()
    {
        // 销毁互斥锁
        pthread_mutex_destroy(&m_mutex);
    }
    
    bool lock()
    {
        // 加锁，成功返回 true
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock()
    {
        // 解锁，成功返回 true
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t *get()
    {
        // 返回底层互斥锁指针，供条件变量等使用
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex; // 底层互斥锁对象
};


class cond {
public:
    cond()
    {
        // 初始化条件变量，失败则抛出异常
        if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            throw std::exception();
        }
    }
    ~cond()
    {
        // 销毁条件变量
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t *m_mutex)
    {
        // 等待条件变量（阻塞），要求传入已加锁的互斥锁指针
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0; // 成功返回 true
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        // 带超时的等待，超时或出错返回 false，成功返回 true
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return ret == 0;
    }
    bool signal()
    {
        // 唤醒一个等待该条件的线程，成功返回 true
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast()
    {
        // 唤醒所有等待该条件的线程，成功返回 true
        return pthread_cond_broadcast(&m_cond) == 0;
    }
private:
    pthread_cond_t m_cond; // 底层条件变量对象
};

#endif