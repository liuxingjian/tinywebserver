#include "epoller.h"

Epoller::Epoller(int maxEvernt) : epollfd(epoll_create(512)), events(maxEvernt)
{
    assert(epollfd >= 0 && events.size() > 0);
}

Epoller::~Epoller()
{
    if (epollfd >= 0)
        close(epollfd);
}

// 添加一个文件描述符到 epoll 监听列表
bool Epoller::addfd(int fd, uint32_t events)
{
    // 如果文件描述符非法，直接返回 false
    if (fd < 0)
        return false;
    // 创建并初始化 epoll_event 结构体
    epoll_event ev = {0};
    // 设置事件关联的文件描述符
    ev.data.fd = fd;
    // 设置需要监听的事件类型
    ev.events = events;
    // 调用 epoll_ctl 添加事件，成功返回 true，否则返回 false
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == 0;
}



bool Epoller::modfd(int fd, uint32_t events)
{
    // 如果文件描述符非法，返回false
    if (fd < 0) return false;
    epoll_event ev = {0};        // 初始化epoll事件结构体
    ev.data.fd = fd;             // 设置事件关联的文件描述符
    ev.events = events;          // 设置新的事件类型
    // 调用epoll_ctl修改文件描述符的监听事件，成功返回true
    return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool Epoller::delfd(int fd)
{
    // 如果文件描述符非法，返回false
    if (fd < 0) return false;
    epoll_event ev = {0};        // 初始化epoll事件结构体
    ev.data.fd = fd;             // 设置事件关联的文件描述符
    // 调用epoll_ctl从epoll实例中删除文件描述符，成功返回true
    return epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev) == 0;
}

int Epoller::wait(int timeoutMs)
{
    // 等待epoll事件，返回触发的事件数
    return epoll_wait(epollfd, &events[0], (int)events.size(), timeoutMs);
}

int Epoller::getEventfd(size_t i) const
{
    // 检查索引合法性
    assert(i < events.size() && i >= 0);
    // 返回第i个事件关联的文件描述符
    return events[i].data.fd;
}

uint32_t Epoller::getEvents(size_t i) const
{
    // 检查索引合法性
    assert(i < events.size() && i >= 0);
    // 返回第i个事件的事件类型掩码
    return events[i].events;
}