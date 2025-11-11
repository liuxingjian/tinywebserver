/**
 * @file epoller.h
 * @brief 定义用于管理基于 epoll 的事件轮询的 Epoller 类。
 *
 * Epoller 类封装了 Linux 的 epoll API，提供添加、修改、删除文件描述符以及等待事件的方法，
 * 简化了网络服务器的事件驱动编程。
 *
 * 用法说明：
 *   - 创建 Epoller 对象并指定最大事件数。
 *   - 使用 addfd()、modfd()、delfd() 管理文件描述符。
 *   - 调用 wait() 进行事件轮询，并通过 getEventfd() 和 getEvents() 获取事件信息。
 *
 * @author
 * @date
 */
#ifndef EPOLLER_H
#define EPOLLER_H



#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <errno.h>

using namespace std;

/**
 * @class Epoller
 * @brief 封装 epoll 事件管理的类。
 */
/**
 * @brief 构造函数，初始化 epoll 实例。
 * @param maxEvent 最大监听事件数，默认 1024。
 */
/**
 * @brief 析构函数，释放 epoll 相关资源。
 */
/**
 * @brief 向 epoll 实例添加文件描述符及其监听事件。
 * @param fd 文件描述符
 * @param events 监听的事件类型（如 EPOLLIN, EPOLLOUT）
 * @return 添加成功返回 true，否则返回 false
 */
/**
 * @brief 修改已添加文件描述符的监听事件。
 * @param fd 文件描述符
 * @param events 新的监听事件类型
 * @return 修改成功返回 true，否则返回 false
 */
/**
 * @brief 从 epoll 实例中删除文件描述符。
 * @param fd 文件描述符
 * @return 删除成功返回 true，否则返回 false
 */
/**
 * @brief 等待 epoll 事件的发生。
 * @param timeoutsMs 超时时间（毫秒），默认 -1 表示无限等待
 * @return 返回发生事件的数量
 */
/**
 * @brief 获取第 i 个事件对应的文件描述符。
 * @param i 事件索引
 * @return 文件描述符
 */
/**
 * @brief 获取第 i 个事件的事件类型。
 * @param i 事件索引
 * @return 事件类型（如 EPOLLIN, EPOLLOUT）
 */
/**
 * @brief epoll 文件描述符。
 */
/**
 * @brief 存储 epoll 事件的容器。
 */
class Epoller
{
public:
    explicit Epoller(int maxEvent = 1024);
    ~Epoller();

    bool addfd(int fd, uint32_t events);
    bool modfd(int fd, uint32_t events);
    bool delfd(int fd);

    int wait(int timeoutsMs = -1);

    int getEventfd(size_t i) const;
    uint32_t getEvents(size_t i) const;
private:
    int epollfd;

    vector<struct epoll_event> events;
};

#endif