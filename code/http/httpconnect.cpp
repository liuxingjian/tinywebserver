/**
 * @file httpconnect.cpp
 * @brief HttpConnect类的实现，用于管理HTTP连接。
 *
 * 本文件包含HttpConnect类的实现，负责在简单Web服务器中处理HTTP连接的生命周期和I/O操作。
 * 主要功能包括连接初始化、读写数据、请求解析、响应生成以及连接关闭。
 *
 * 主要功能点：
 * - 使用套接字文件描述符和客户端地址初始化连接。
 * - 使用原子变量管理活跃连接数，保证线程安全。
 * - 支持边缘触发（ET）模式下的读写操作。
 * - 解析HTTP请求并生成相应的响应。
 * - 利用分散/聚集I/O（writev）高效发送响应。
 * - 连接关闭时进行资源清理和日志记录。
 *
 * 使用方法：
 * 1. 通过init()初始化连接。
 * 2. 使用read()和write()进行I/O操作。
 * 3. 调用process()处理请求并准备响应。
 * 4. 完成后通过closeConnect()关闭连接。
 *
 * 线程安全：
 * - 活跃连接数通过std::atomic管理，支持并发访问。
 *
 * 日志记录：
 * - 连接事件和请求处理过程均有日志，便于监控和调试。
 */
#include "httpconnect.h"
using namespace std;

// 静态成员变量初始化
const char* HttpConnect::srcDir;
atomic<int> HttpConnect::userCnt;
bool HttpConnect::isET;

// 构造函数，初始化成员变量
HttpConnect::HttpConnect()
{
    fd = -1;                // 文件描述符初始化为-1
    addr = {0};             // 地址结构体清零
    isClose = true;         // 标记连接已关闭
}

/* 连接初始化：套接字，端口，缓存，请求解析状态机 */
void HttpConnect::init(int fd, const sockaddr_in& addr)
{
    assert(fd > 0);         // 断言文件描述符有效
    userCnt ++;             // 活跃连接数加一
    this->addr = addr;      // 保存客户端地址
    this->fd = fd;          // 保存文件描述符
    writeBuffer.retrieveAll(); // 清空写缓存
    readBuffer.retrieveAll();  // 清空读缓存
    isClose = false;        // 标记连接未关闭
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd, getIP(), getPort(), (int)userCnt); // 记录连接日志
    request.init();         // 初始化请求解析状态机
}

/* 关闭连接 */
void HttpConnect::closeConnect()
{
    response.unmapFile();   // 解除文件映射
    if (!isClose)           // 如果连接未关闭
    {
        isClose = true;     // 标记连接关闭
        userCnt--;          // 活跃连接数减一
        close(fd);          // 关闭文件描述符
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd, getIP(), getPort(), (int)userCnt); // 记录断开日志
    }
}

/* 读方法，ET模式会将缓存读空 */
// 返回最后一次读取的长度，以及错误类型
ssize_t HttpConnect::read(int* saveErrno)
{
    ssize_t len = -1;       // 最后一次读取的长度
    do
    {
        len = readBuffer.readfd(fd, saveErrno); // 从fd读取数据到读缓存
        if (len <= 0)       // 读取失败或连接关闭
        {
            *saveErrno = errno; // 保存错误码
            break;
        }
    } while (isET);         // 边缘触发模式下循环读取直到读空
    return len;
}

/* 写方法,响应头和响应体是分开的，要用iov实现写操作 */
ssize_t HttpConnect::write(int* saveErrno)
{
    ssize_t len = -1;       // 最后一次写入的长度
    do
    {
        len = writev(fd, iov, iovCnt); // 分散写，将响应头和体一起写入
        if (len <= 0)       // 写入失败
        {
            *saveErrno = errno; // 保存错误码
            break;
        }
        // 缓存为空，传输完成
        if (iov[0].iov_len + iov[1].iov_len == 0) break;
        // 响应头已经传输完成
        else if ((size_t)len > iov[0].iov_len)
        {
            // 更新响应体传输起点和长度
            iov[1].iov_base = (uint8_t*)iov[1].iov_base + (len - iov[0].iov_len);
            iov[1].iov_len -= (len - iov[0].iov_len);
            // 响应头不再需要传输
            if (iov[0].iov_len)
            {
                iov[0].iov_len = 0; // 响应头长度置零
                writeBuffer.retrieveAll(); // 清空写缓存
            }
        }
        // 响应头还没传输完成
        else
        {
            // 更新响应头传输起点和长度
            iov[0].iov_base = (uint8_t*)iov[0].iov_base + len;
            iov[0].iov_len -= len;
            writeBuffer.retrieve(len); // 回收已写入的数据
        }
    } while (isET || toWriteBytes() > 10240); // 边缘触发或待写数据大于10MB时继续写
    return len;
}

/* 处理方法：解析读缓存内的请求报文，判断是否完整 */
// 不完整返回false，完整在写缓存内写入响应头，并获取响应体内容（文件）
bool HttpConnect::process()
{
    // 如果读缓存为空，直接返回false
    if (readBuffer.readableBytes() <= 0) return false;

    HTTP_CODE ret = request.parse(readBuffer); // 解析HTTP请求
    // 请求不完整，继续读
    if (ret == HTTP_CODE::NO_REQUEST)
    {
        return false; // 返回false后，会继续监听读
    }
    // 请求完整，开始写
    else if (ret == HTTP_CODE::GET_REQUEST)
    {
        LOG_DEBUG("%s", request.getPathConst().c_str()); // 打印请求路径
        response.init(srcDir, request.getPath(), request.isKeepAlive(), 200); // 初始化响应
        request.init(); // 如果是长连接，等待下一次请求，需要初始化
    }
    // 请求行错误, bad request
    else if (ret == HTTP_CODE::BAD_REQUEST)
    {
        response.init(srcDir, request.getPath(), false, 400); // 初始化400响应
    }

    response.makeResponse(writeBuffer); // 生成响应头到写缓存
    // 响应头
    iov[0].iov_base = (char*)writeBuffer.peek(); // 指向响应头数据
    iov[0].iov_len = writeBuffer.readableBytes(); // 响应头长度
    iovCnt = 1; // iov数量为1
    // 响应体
    if (response.getFileLen() > 0 && response.getFile())
    {
        iov[1].iov_base = response.getFile(); // 指向响应体数据
        iov[1].iov_len = response.getFileLen(); // 响应体长度
        iovCnt = 2; // iov数量为2
    }

    LOG_DEBUG("filesize:%d, iovcnt:%d, write:%d bytes", response.getFileLen(), iovCnt, toWriteBytes()); // 打印响应信息
    return true;
}
