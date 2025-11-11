/**
 * @file webserver.cpp
 * @brief WebServer类的实现，一个使用epoll和线程池的简单多线程Web服务器。
 *
 * WebServer类负责服务器初始化、连接管理、事件处理和资源清理。
 * 支持端口、触发模式、超时时间、数据库连接池、线程池和日志等参数配置。
 *
 * 主要功能：
 * - 初始化服务器资源，包括线程池、数据库连接池和日志系统。
 * - 设置监听和连接套接字的epoll事件模式。
 * - 使用epoll处理新连接、读写和错误事件。
 * - 管理客户端连接和定时器，实现连接超时处理。
 * - 处理HTTP请求并生成响应。
 * - 支持优雅关闭和资源释放。
 *
 * 关键方法：
 * - WebServer::WebServer(...) : 构造函数，初始化服务器资源。
 * - WebServer::~WebServer()   : 析构函数，清理资源。
 * - void initEventMode(int trigMode) : 配置epoll事件模式。
 * - void start()              : 主事件循环，处理epoll事件。
 * - void sendError(int fd, const char* info) : 向客户端发送错误信息。
 * - void closeConnect(HttpConnect* client)   : 关闭客户端连接并从epoll移除。
 * - void addClient(int fd, sockaddr_in addr) : 注册新客户端并设置定时器。
 * - void dealListen()         : 接受新连接。
 * - void dealRead(HttpConnect* client) : 处理读事件，交由线程池。
 * - void dealWrite(HttpConnect* client) : 处理写事件，交由线程池。
 * - void extentTime(HttpConnect* client) : 重置连接定时器。
 * - void onRead(HttpConnect* client)     : 读取客户端数据。
 * - void onProcess(HttpConnect* client)  : 处理HTTP请求。
 * - void onWrite(HttpConnect* client)    : 向客户端发送响应。
 * - bool initSocket()         : 初始化监听套接字并注册到epoll。
 * - int setfdNonblock(int fd) : 设置套接字为非阻塞模式。
 *
 * 线程安全：
 * - 使用线程池并发处理请求。
 * - epoll实现高效事件驱动I/O。
 *
 * 依赖：
 * - 需要支持类：HttpConnect、ThreadPool、SqlConnPool、HeapTimer、Epoller、Log。
 *
 * 用法：
 * - 按需配置参数实例化WebServer，调用start()运行服务器。
 */
#include "webserver.h"
using namespace std;

// 服务器相关参数包括连接参数、数据库参数、线程池参数、日志参数
WebServer::WebServer(
    int port, int trigMode, int timeoutMs, bool optLinger,
    int sqlPort, const char* sqlUser, const char* sqlPwd,
    const char* dbName, int connPoolNum,
    int threadNum, int maxRequests,
    bool openLog, int logLevel, int logQueSize):
    port(port), openLinger(optLinger), timeoutMs(timeoutMs), shutdown(false),
    timer(new HeapTimer()), epoller(new Epoller())
{
    // 获取当前工作目录，动态分配缓存
    srcDir = getcwd(nullptr, 256);
    assert(srcDir);
    // 拼接资源目录路径
    strncat(srcDir, "/resources/", 16);
    // 初始化HttpConnect静态成员
    HttpConnect::userCnt = 0;
    HttpConnect::srcDir = srcDir;
    // 初始化线程池
    ThreadPool::instance()->init(threadNum, maxRequests);
    // 初始化数据库连接池
    SqlConnPool::instance()->init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    // 设置epoll事件触发模式
    initEventMode(trigMode);
    // 初始化监听套接字
    if (!initSocket()) shutdown = true;

    // 初始化日志系统
    if(openLog) {
        Log::instance()->init(logLevel, "./log", ".log", logQueSize);
        if(shutdown) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port, optLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent & EPOLLET ? "ET": "LT"),
                            (connEvent & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConnect::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

// 析构函数，释放资源
WebServer::~WebServer()
{
    // 关闭监听套接字
    close(listenfd);
    shutdown = true;
    // 释放路径缓存
    free(srcDir);
    // 销毁数据库连接池
    SqlConnPool::instance()->destroy();
}

/* 设置不同套接字的触发模式 */
void WebServer::initEventMode(int trigMode)
{
    // 初始化监听事件
    listenEvent = EPOLLRDHUP;
    // 初始化连接事件，oneshot和对端断开
    connEvent = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent |= EPOLLET;
        break;
    case 2:
        listenEvent |= EPOLLET;
        break;
    case 3:
        listenEvent |= EPOLLET;
        connEvent |= EPOLLET;
        break;
    default:
        listenEvent |= EPOLLET;
        connEvent |= EPOLLET;
        break;
    }
    // 判断连接套接字是否为ET模式
    HttpConnect::isET = (connEvent & EPOLLET);
}

/* epoll循环监听事件，根据事件类型调用相应方法 */
void WebServer::start()
{
    int timeMs = -1;
    if (!shutdown) {LOG_INFO("========= Server start =========");}
    while (!shutdown)
    {
        // 获取下一个计时器超时时间
        if (timeoutMs > 0)
            timeMs = timer->getNextTick();
        // epoll等待事件
        int eventCnt = epoller->wait(timeMs);
        // 遍历所有事件
        for (int i = 0; i < eventCnt; i ++)
        {
            int fd = epoller->getEventfd(i);         // 获取事件fd
            uint32_t events = epoller->getEvents(i); // 获取事件类型
            if (fd == listenfd) dealListen();        // 监听套接字处理新连接
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) // 连接断开或错误
            {
                assert(users.count(fd) > 0);
                closeConnect(&users[fd]);
            }
            else if (events & EPOLLIN)               // 读事件
            {
                assert(users.count(fd) > 0);
                dealRead(&users[fd]);
            }
            else if (events & EPOLLOUT)              // 写事件
            {
                assert(users.count(fd) > 0);
                dealWrite(&users[fd]);
            }
            else
            {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

/* 发送错误信息到客户端并关闭连接 */
void WebServer::sendError(int fd, const char* info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0)
    {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

/* 关闭连接套接字，并从epoll事件表中删除 */
void WebServer::closeConnect(HttpConnect* client)
{
    assert(client);
    LOG_INFO("Client[%d] quit!", client->getfd());
    epoller->delfd(client->getfd());
    client->closeConnect();
}

/* 注册新客户端并设置定时器 */
void WebServer::addClient(int fd, sockaddr_in addr)
{
    assert(fd > 0);
    users[fd].init(fd, addr); // 初始化HttpConnect对象
    if(timeoutMs > 0)
    {
        timer->add(fd, timeoutMs, bind(&WebServer::closeConnect, this, &users[fd]));
    }
    epoller->addfd(fd, EPOLLIN | connEvent); // 注册到epoll
    setfdNonblock(fd);                       // 设置非阻塞
    LOG_INFO("Client[%d] in!", users[fd].getfd());
}

/* 处理新连接，ET模式下循环读完连接队列 */
void WebServer::dealListen()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do
    {
        int fd = accept(listenfd, (struct sockaddr*)&addr, &len);
        if (fd <= 0) return;
        if (HttpConnect::userCnt >= MAX_FD)
        {
            sendError(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        addClient(fd, addr);
    } while (listenEvent & EPOLLET);
}

/* 读事件加入线程池任务队列 */
void WebServer::dealRead(HttpConnect* client)
{
    assert(client);
    extentTime(client); // 重置计时器
    ThreadPool::instance()->addTask(std::bind(&WebServer::onRead, this, client));
}

/* 写事件加入线程池任务队列 */
void WebServer::dealWrite(HttpConnect* client)
{
    assert(client);
    extentTime(client); // 重置计时器
    ThreadPool::instance()->addTask(std::bind(&WebServer::onWrite, this, client));
}

// 重置连接计时器
void WebServer::extentTime(HttpConnect* client)
{
    assert(client);
    if (timeoutMs > 0)
        timer->adjust(client->getfd(), timeoutMs);
}

/* 读函数：接收数据并处理 */
void WebServer::onRead(HttpConnect* client)
{
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    // 客户端关闭连接
    if (ret <= 0 && readErrno != EAGAIN)
    {
        closeConnect(client);
        return;
    }
    onProcess(client); // 处理请求
}

/* 处理HTTP请求，决定监听读还是写 */
void WebServer::onProcess(HttpConnect* client)
{
    if (client->process())
    {
        epoller->modfd(client->getfd(), connEvent | EPOLLOUT); // 响应准备好，监听写
    }
    else
    {
        epoller->modfd(client->getfd(), connEvent | EPOLLIN);  // 请求不完整，继续读
    }
}

/* 写函数：发送响应报文 */
void WebServer::onWrite(HttpConnect* client)
{
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    // 发送完毕
    if (client->toWriteBytes() == 0)
    {
        if (client->isKeepAlive())
        {
            onProcess(client); // 长连接，继续处理
            return;
        }
    }
    // 发送失败
    else if (ret < 0)
    {
        if (writeErrno == EAGAIN)
        {
            epoller->modfd(client->getfd(), connEvent | EPOLLOUT); // 缓冲区满，继续监听写
            return;
        }
    }
    // 其他错误，关闭连接
    closeConnect(client);
}

/* 初始化监听套接字并注册到epoll */
bool WebServer::initSocket()
{
    int ret;
    struct sockaddr_in addr;
    if (port > 65536 || port < 1024)
    {
        LOG_ERROR("port: %d error!", port);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    struct linger optLinger = {0};
    if (openLinger)
    {
        optLinger.l_onoff = 1;
        optLinger.l_linger = 20; // 优雅关闭，等待20秒
    }

    listenfd = socket(AF_INET, SOCK_STREAM, 0); // 创建套接字
    if (listenfd < 0)
    {
        LOG_ERROR("port: %d create socket error!", port);
        return false;
    }
    ret = setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger)); // 设置优雅关闭
    if (ret == -1)
    {
        close(listenfd);
        LOG_ERROR("port: %d init linger error!", port);
        return false;
    }

    int optval = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)); // 端口复用
    if (ret == -1)
    {
        LOG_ERROR("set socket error!");
        close(listenfd);
        return false;
    }
    ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)); // 绑定端口
    if (ret == -1)
    {
        LOG_ERROR("bind port: %d error!", port);
        close(listenfd);
        return false;
    }
    ret = listen(listenfd, 6); // 监听端口
    if (ret == -1)
    {
        LOG_ERROR("listen port: %d error!", port);
        close(listenfd);
        return false;
    }
    ret = epoller->addfd(listenfd, listenEvent | EPOLLIN); // 注册到epoll
    if (ret == 0)
    {
        LOG_ERROR("Add listen error!");
        close(listenfd);
        return false;
    }
    setfdNonblock(listenfd); // 设置非阻塞
    LOG_INFO("Server port: %d", port);
    return true;
}

/* 设置套接字为非阻塞 */
int WebServer::setfdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
