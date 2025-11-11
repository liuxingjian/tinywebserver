#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include <vector>
#include <atomic>
#include <cassert>

using namespace std;

/**
 * @brief 一个线程安全的缓冲区类，用于高效的数据存储和操作。
 *
 * Buffer 类提供了追加、读取和管理数据的方法，数据存储在连续的内存块中。
 * 该类适用于网络编程场景，例如 Web 服务器，可以实现高效的输入输出操作。
 *
 * @note 所有公共方法都是线程安全的，因为读写位置使用了原子变量进行管理。
 *
 * 主要功能包括：
 * - 追加字符串、字节数组或其他 Buffer 对象的数据；
 * - 支持读取指定长度的数据或全部数据，并自动维护读写指针；
 * - 可以直接从文件描述符读取数据或写入数据，适合与 socket 结合使用；
 * - 内部自动扩展缓冲区空间，保证写入操作不会越界；
 * - 提供查询可读、可写、可预留空间的接口，方便高效的数据操作。
 */


class Buffer
{
public:
    Buffer(int initBuffSize = 1024); // 构造函数，初始化缓冲区大小
    ~Buffer() = default; // 析构函数，默认实现

    size_t writableBytes() const;    // 获取可写空间字节数
    size_t readableBytes() const;    // 获取可读数据字节数
    size_t prependableBytes() const; // 获取前置空间字节数

    const char* peek() const;        // 获取当前可读数据的起始指针
    void ensureWritable(size_t len); // 确保有足够的可写空间
    void hasWritten(size_t len);     // 写入数据后更新写指针

    void retrieve(size_t len);               // 回收指定长度的数据
    void retrieveUntil(const char* end);     // 回收直到指定指针的数据

    void retrieveAll();                      // 回收所有数据
    string retrieveAllToStr();               // 获取所有可读数据并回收，返回字符串

    const char* beginWriteConst() const;     // 获取可写区域的常量指针
    char* beginWrite();                      // 获取可写区域的指针

    void append(const string& str);                  // 追加字符串数据
    void append(const char* str, size_t len);        // 追加字符数组数据
    void append(const void* data, size_t len);       // 追加任意数据块
    void append(const Buffer& buffer);               // 追加另一个缓冲区的数据

    ssize_t readfd(int fd, int* Errno);      // 从文件描述符读取数据到缓冲区
    ssize_t writefd(int fd, int* Errno);     // 将缓冲区数据写入文件描述符

private:
    char* beginPtr();                        // 获取底层数据指针
    const char* beginPtr() const;            // 获取底层数据常量指针
    void makeSpace(size_t len);              // 扩展缓冲区空间

    vector<char> buffer;                     // 字节缓冲区
    atomic<size_t> readPos;                  // 读指针位置（原子操作，保证线程安全）
    atomic<size_t> writePos;                 // 写指针位置（原子操作，保证线程安全）
};

#endif