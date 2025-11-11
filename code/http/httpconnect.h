/**
 * @file httpconnect.h
 * @brief HttpConnect类声明，用于管理HTTP连接。
 *
 * 此头文件定义了HttpConnect类，封装了Web服务器中处理单个HTTP连接的逻辑。
 * 提供了初始化连接、读写数据、关闭连接、处理HTTP请求和响应的方法。
 * 该类还管理输入输出缓冲区，并支持长连接（keep-alive）等特性。
 *
 * 依赖：
 *  - 用于网络和I/O操作的系统头文件。
 *  - 项目相关模块：日志、SQL连接池、缓冲区、HTTP请求与响应处理。
 *
 * 用法：
 *  - 每个客户端连接创建一个HttpConnect实例。
 *  - 使用init()方法初始化连接（传入socket和地址）。
 *  - 使用read()和write()进行数据传输。
 *  - 使用process()处理HTTP请求和响应逻辑。
 *  - 使用closeConnect()安全关闭连接。
 */
#ifndef HTTPCONNECT_H
#define HTTPCONNECT_H

#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>

#include "../log/log.h"
#include "../sqlconnpool/sql_conn_pool.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

// 引入标准命名空间，避免写 std:: 前缀（建议实际项目中不要这样做，容易污染全局命名空间）
using namespace std;

// HttpConnect 类声明，负责 HTTP 连接的管理
class HttpConnect
{
public:
    // 构造函数，初始化连接对象
    HttpConnect();
    // 析构函数，销毁对象时关闭连接
    ~HttpConnect() {closeConnect();}

    // 初始化连接，设置 socket 文件描述符和地址
    void init(int sockfd, const sockaddr_in& addr);

    // 读取数据，返回读取的字节数，错误码保存在 saveErrno
    ssize_t read(int* saveErrno);
    // 写入数据，返回写入的字节数，错误码保存在 saveErrno
    ssize_t write(int* saveErrno);

    // 关闭连接
    void closeConnect();

    // 获取 socket 文件描述符
    int getfd() const {return fd;}
    // 获取连接地址
    sockaddr_in getAddr() const {return addr;}
    // 获取端口号（注意：sin_port 是网络字节序，可能需要 ntohs 转换）
    int getPort() const {return addr.sin_port;}
    // 获取 IP 地址字符串（inet_ntoa 返回静态字符串，线程不安全）
    const char* getIP() const {return inet_ntoa(addr.sin_addr);}

    // 处理 HTTP 请求，返回处理结果
    bool process();

    // 计算待写入的总字节数（iov[0] 和 iov[1] 的长度之和）
    int toWriteBytes()
    {
        return iov[0].iov_len + iov[1].iov_len;
    }

    // 判断是否为长连接（Keep-Alive）
    bool isKeepAlive() const
    {
        return request.isKeepAlive();
    }

    // 静态成员，是否为边缘触发模式（ET）
    static bool isET;
    // 静态成员，资源目录路径
    static const char* srcDir;
    // 静态成员，用户计数（原子操作，线程安全）
    static atomic<int> userCnt;

private:
    // socket 文件描述符
    int fd;
    // 客户端地址信息
    struct sockaddr_in addr;

    // 连接是否关闭
    bool isClose;

    // iovec 数组元素个数
    int iovCnt;
    // iovec 数组，用于分散/聚集 I/O
    struct iovec iov[2];

    // 读缓冲区
    Buffer readBuffer;
    // 写缓冲区
    Buffer writeBuffer;

    // HTTP 请求对象
    HttpRequest request;
    // HTTP 响应对象
    HttpResponse response;
};

// 头文件结尾，防止重复包含
#endif