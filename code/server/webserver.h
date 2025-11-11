/**
 * @file webserver.h
 * @brief WebServer类声明，用于管理HTTP连接和服务器操作。
 *
 * WebServer类封装了初始化套接字、事件处理、客户端连接管理，
 * 并集成了线程池、日志、SQL连接池和定时器等功能。
 * 提供了启动服务器、处理读写事件、管理连接生命周期和错误处理的方法。
 *
 * 依赖项：
 *  - epoller.h：基于epoll的事件处理。
 *  - log.h：日志工具。
 *  - sql_conn_pool.h：SQL连接池管理。
 *  - threadpool.h：并发请求处理的线程池。
 *  - httpconnect.h：HTTP连接抽象。
 *  - heaptimer.h：连接超时的定时器管理。
 *
 * 用法：
 *  - 使用所需配置参数实例化WebServer。
 *  - 调用start()方法开始处理HTTP请求。
 *
 * @class WebServer
 * @brief 管理HTTP服务器操作、客户端连接和事件处理。
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../log/log.h"
#include "../sqlconnpool/sql_conn_pool.h"
#include "../threadpool/threadpool.h"
#include "../http/httpconnect.h"
#include "../timer/heaptimer.h"

using namespace std;

// WebServer类声明
class WebServer{
 public:
    // 构造函数，初始化服务器参数
    WebServer(
        int port, int trigMode, int timeoutMs, bool optLinger,
        int sqlPort, const char* sqlUser, const char* sqlPwd,
        const char* dbName, int connPoolNum,
        int threadNum, int maxRequests,
        bool openLog, int logLevel, int logQueSize);
    
    // 析构函数，释放资源
    ~WebServer();

    // 启动服务器
    void start();  

private:
    // 初始化监听套接字
    bool initSocket();
    // 初始化事件触发模式
    void initEventMode(int trigMode);
    // 添加客户端连接
    void addClient(int fd, sockaddr_in addr);

    // 处理监听事件
    void dealListen();
    // 处理写事件
    void dealWrite(HttpConnect* client);
    // 处理读事件
    void dealRead(HttpConnect* client);

    // 发送错误信息到客户端
    void sendError(int fd, const char* info);
    // 延长连接定时器时间
    void extentTime(HttpConnect* client);
    // 关闭客户端连接
    void closeConnect(HttpConnect* client);

    // 读事件回调
    void onRead(HttpConnect* client);
    // 写事件回调
    void onWrite(HttpConnect* client);
    // 处理请求回调
    void onProcess(HttpConnect* client);

    // 最大文件描述符数量
    static const int MAX_FD = 65536;
    // 设置文件描述符为非阻塞
    static int setfdNonblock(int fd);

    int port;                      // 服务器端口
    bool openLinger;               // 是否启用优雅关闭
    int timeoutMs;                 // 超时时间（毫秒）
    bool shutdown;                 // 服务器关闭标志
    int listenfd;                  // 监听套接字
    char* srcDir;                  // 资源目录

    uint32_t listenEvent;          // 监听事件类型
    uint32_t connEvent;            // 连接事件类型

    unique_ptr<HeapTimer> timer;   // 定时器
    unique_ptr<Epoller> epoller;   // epoll事件处理器
    unordered_map<int, HttpConnect> users; // 用户连接映射表
};

#endif