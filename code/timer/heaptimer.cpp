/**
 * @file heaptimer.cpp
 * @brief 基于小根堆的定时器管理器实现。
 *
 * 本文件实现了 HeapTimer 类，使用小根堆管理定时器。
 * 每个定时器关联唯一 ID、过期时间和超时回调函数，到期后自动执行回调。
 * 堆结构保证能高效获取最近即将过期的定时器。
 *
 * HeapTimer 支持以下操作：
 * - 添加新定时器或更新已有定时器。
 * - 调整定时器的过期时间。
 * - 删除定时器。
 * - 执行所有已过期定时器的回调。
 * - 清空所有定时器。
 * - 获取下一个定时器的剩余时间。
 *
 * 内部机制：
 * - 使用 vector 模拟二叉堆（小根堆）。
 * - 通过哈希表快速定位定时器 ID。
 * - 提供上浮（siftup）和下沉（siftdown）操作维护堆性质。
 * - 保证定时器操作高效且一致。
 *
 * 主要函数说明：
 * - swapNode(int i, int j)：交换堆中两个节点并更新哈希表索引。
 * - siftup(int i)：节点上浮，恢复堆序。
 * - siftdown(int i)：节点下沉，恢复堆序。
 * - add(int id, int timeout, const TimeoutCallBack& cb)：添加或更新定时器。
 * - del(int i)：删除指定位置的定时器。
 * - adjust(int id, int timeout)：调整定时器过期时间。
 * - tick()：执行所有已过期定时器的回调。
 * - pop()：移除堆顶定时器。
 * - clear()：清空所有定时器。
 * - getNextTick()：返回下一个定时器的剩余时间。
 *
 * 使用场景：
 * - 适用于事件驱动服务器高效管理超时任务。
 * - 保证超时定时器及时处理，新定时器管理开销低。
 */
#include "heaptimer.h"

/* 数组模拟堆
堆是一个完全二叉树，每个点都小于等于左右结点，根结点也即堆顶上全部数据的最小值（小根堆）
如果用0号作为根结点，那么结点x的左结点是 2x+1，右结点是2x+2
那么结点x的父结点就是 (x-1)/2

swap操作：交换两个结点，同时更新哈希表内每个定时器的下标
down操作：比左或右结点大，与左右结点的最小值交换
up操作：比父结点小，与父结点交换

我们的需求有以下几个：
增加定时器：查哈希表，如果是新定时器，插在最后，再up，如果不是新定时器，就需要调整定时器
调整定时器：更新定时后，再执行down和up（实际上只会执行一个）
删除定时器：与最后一个元素交换，删除末尾元素，然后再down和up（删除任意位置结点，同样也只会执行一个）
*/
// 交换堆中两个节点，并更新哈希表中的索引
void HeapTimer::swapNode(int i, int j)
{
    int s = heap.size();
    assert(i >= 0 && i < s); // 检查下标合法
    assert(j >= 0 && j < s);
    std::swap(heap[i], heap[j]); // 交换堆节点
    ref[heap[i].id] = i;         // 更新哈希表索引
    ref[heap[j].id] = j;
}

// 节点上浮操作，恢复堆序
void HeapTimer::siftup(int i)
{
    assert(i >= 0 && i < (int)heap.size());
    int j = (i - 1) / 2; // 父节点下标
    while(j >= 0 && heap[i] < heap[j]) // 当前节点比父节点小则交换
    {
        swapNode(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

// 节点下沉操作，恢复堆序
void HeapTimer::siftdown(int i)
{
    int s = heap.size();
    assert(i >= 0 && i < s);
    int t = i * 2 + 1; // 左子节点下标

    while (t < s)
    {
        // 找到左右子节点中较小的一个
        if (t + 1 < s && heap[t + 1] < heap[t]) t++;
        if (heap[i] < heap[t]) break; // 当前节点已小于子节点，结束
        swap(heap[i], heap[t]);       // 交换节点
        i = t, t = i * 2 + 1;         // 继续下沉
    }
}

// 添加新定时器或更新已有定时器
void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb)
{
    assert(id >= 0);
    int i;
    if(!ref.count(id)) // 新定时器
    {
        i = heap.size();
        ref[id] = i;
        heap.push_back({id, Clock::now() + MS(timeout), cb}); // 添加到堆尾
        siftup(i); // 上浮恢复堆序
    } 
    else // 已有定时器，更新
    {
        i = ref[id];
        heap[i].expires = Clock::now() + MS(timeout); // 更新过期时间
        heap[i].cb = cb;                              // 更新回调
        siftdown(i); // 下沉恢复堆序
        siftup(i);   // 上浮恢复堆序
    }
}

// 删除指定位置的定时器
void HeapTimer::del(int i)
{
    assert(!heap.empty() && i >= 0 && i < (int)heap.size());
    int n = heap.size() - 1;
    swapNode(i, n); // 与最后一个节点交换

    ref.erase(heap.back().id); // 移除哈希表索引
    heap.pop_back();           // 删除堆尾节点
    // 如果堆不为空，调整堆序
    if (!heap.empty())
    {
        siftdown(i);
        siftup(i);
    }
}

// 调整定时器的过期时间
void HeapTimer::adjust(int id, int timeout)
{
    assert(!heap.empty() && ref.count(id));
    heap[ref[id]].expires = Clock::now() + MS(timeout); // 更新过期时间
    siftdown(ref[id]); // 下沉恢复堆序
    siftup(ref[id]);   // 上浮恢复堆序
}

// 执行所有已过期定时器的回调
void HeapTimer::tick()
{
    if (heap.empty()) return;

    while (!heap.empty())
    {
        TimerNode node = heap.front(); // 堆顶定时器

        // 如果堆顶定时器还未过期，跳出循环
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0)
        {
            break;
        }
        node.cb(); // 执行回调
        pop();     // 移除堆顶定时器
    }
}

// 移除堆顶定时器
void HeapTimer::pop()
{
    assert(!heap.empty());
    del(0);
}

// 清空所有定时器
void HeapTimer::clear() 
{
    ref.clear();
    heap.clear();
}

// 获取下一个定时器的剩余时间
int HeapTimer::getNextTick()
{
    // 处理堆顶计时器，若超时执行回调再删除
    tick();
    int res = -1;
    if(!heap.empty())
    {
        // 计算堆顶定时器剩余时间
        res = std::chrono::duration_cast<MS>(heap.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; } // 已超时则返回0
    }
    return res;
}