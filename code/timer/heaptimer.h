/**
 * @file heaptimer.h
 * @brief 定义了一个基于堆的定时器管理类，用于处理超时事件。
 *
 * 本头文件提供了 TimerNode 结构体和 HeapTimer 类，用于通过最小堆管理定时器。
 * 每个定时器关联一个唯一标识符、到期时间和超时回调函数。
 *
 * TimerNode:
 *   - 表示单个定时器，包含ID、到期时间戳和回调函数。
 *   - 重载 '<' 运算符，使到期时间更早的定时器优先级更高。
 *
 * HeapTimer:
 *   - 使用最小堆管理多个 TimerNode 对象。
 *   - 支持添加、调整和移除定时器。
 *   - 提供 tick() 方法处理已过期定时器并执行回调。
 *   - 通过映射记录定时器ID与堆下标，实现高效访问。
 *   - 提供堆操作和定时器管理的辅助函数。
 *
 * 用法:
 *   - 使用 add() 添加定时器。
 *   - 使用 adjust() 调整定时器到期时间。
 *   - 定期调用 tick() 处理过期定时器。
 *   - 使用 getNextTick() 获取下一个到期时间间隔。
 */

// 定义头文件保护宏 HEAPTIMER_H
#ifndef HEAPTIMER_H
#define HEAPTIMER_H

// 引入标准库和相关头文件
#include <queue>                // 队列容器
#include <unordered_map>        // 哈希表容器
#include <time.h>               // 时间相关函数
#include <algorithm>            // 算法库
#include <arpa/inet.h>          // 网络相关
#include <functional>           // std::function
#include <assert.h>             // 断言
#include <chrono>               // 高精度计时
#include "../log/log.h"         // 日志模块

// 定义定时器回调类型
typedef std::function<void()> TimeoutCallBack;
// 定义高精度时钟类型
typedef std::chrono::high_resolution_clock Clock;
// 定义毫秒类型
typedef std::chrono::milliseconds MS;
// 定义时间戳类型
typedef Clock::time_point TimeStamp;

// 定时器结点结构体
struct TimerNode 
{
    int id; // 连接套接字描述符
    TimeStamp expires; // 到期时间
    TimeoutCallBack cb; // 回调函数
    // 重载<运算符，到期时间近的排在前面
    bool operator<(const TimerNode& t) 
    {
        return expires < t.expires;
    }
};

// 定时器堆管理类
class HeapTimer 
{
public:
    // 构造函数，预留空间
    HeapTimer() { heap.reserve(64); }

    // 析构函数，清理资源
    ~HeapTimer() { clear(); }
    
    // 调整定时器到期时间
    void adjust(int id, int newExpires);

    // 添加定时器
    void add(int id, int timeOut, const TimeoutCallBack& cb);

    // 清空所有定时器
    void clear();

    // 处理已过期定时器并执行回调
    void tick();

    // 弹出最近到期的定时器
    void pop();

    // 获取最近到期时间间隔
    int getNextTick();

private:
    // 删除指定下标的定时器
    void del(int i);
    
    // 堆向上调整
    void siftup(int i);

    // 堆向下调整
    void siftdown(int i);

    // 交换堆中两个节点
    void swapNode(int i, int j);

    // 定时器堆容器
    std::vector<TimerNode> heap;

    // 定时器ID到堆下标的映射
    std::unordered_map<int, int> ref;
};

#endif