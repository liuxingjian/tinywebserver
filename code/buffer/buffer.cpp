/**
 * @file buffer.cpp
 * @brief Implementation of the Buffer class for efficient memory management in a web server.
 *
 * 内存模型说明:
 * | begin |---prependable---| readPos |---readable---| writePos |---writable---| end |
 * - prependable: 前置空间，未使用区域（begin到readPos）
 * - readable: 可读数据区域（readPos到writePos）
 * - writable: 可写空间区域（writePos到end）
 *
 * 主要功能:
 * - 管理缓冲区的读写指针，实现高效的数据追加、读取和回收
 * - 自动扩容和数据移动，保证写入空间充足
 * - 支持分散读(readv)和直接写(fd)操作
 * - 提供数据追加、清空、回收等接口
 *
 * 主要方法说明:
 * - readableBytes(): 获取可读字节数
 * - writableBytes(): 获取可写字节数
 * - prependableBytes(): 获取前置空间字节数
 * - peek(): 获取当前可读数据的起始指针
 * - retrieve()/retrieveUntil()/retrieveAll(): 回收数据，移动读指针或清空缓冲区
 * - retrieveAllToStr(): 获取所有可读数据并清空缓冲区
 * - append(): 追加数据（字符串、数据块、其他Buffer）
 * - ensureWritable(): 确保有足够可写空间，不足时自动扩容或移动数据
 * - readfd()/writefd(): 从文件描述符读/写数据
 * - makeSpace(): 扩展缓冲区或移动未读数据到前端
 *
 * 线程安全: 本类未实现线程安全，需在多线程环境下加锁保护。
 *
 * @author
 * @date
 */
#include "buffer.h"

using namespace std;

/*
内存模型：
begin---------read--------write--------end
begin-read: prependable   // 前置空间
read-write: readable      // 可读数据
write-end: writable       // 可写空间
*/

// 构造函数，初始化缓冲区大小和读写指针
Buffer::Buffer(int initBuffSize) : buffer(initBuffSize), readPos(0), writePos(0) {}

// 获取可读字节数（写指针-读指针）
size_t Buffer::readableBytes() const
{
    return writePos - readPos;
}

// 获取可写字节数（缓冲区总大小-写指针）
size_t Buffer::writableBytes() const
{
    return buffer.size() - writePos;
}

// 获取前置空间字节数（读指针位置）
size_t Buffer::prependableBytes() const
{
    return readPos;
}

// 获取当前可读数据的起始指针
const char* Buffer::peek() const
{
    return beginPtr() + readPos;
}

// 回收指定长度的数据（移动读指针）
void Buffer::retrieve(size_t len)
{
    assert(len <= readableBytes()); // 检查回收长度合法
    readPos += len;                 // 读指针前移
}

// 回收直到指定指针的数据
void Buffer::retrieveUntil(const char* end)
{
    assert(peek() <= end);          // 检查目标指针合法
    retrieve(end - peek());         // 回收到end位置
}

// 回收所有数据，缓冲区清零，指针归位
void Buffer::retrieveAll()
{
    bzero(&buffer[0], buffer.size()); // 缓冲区内容清零
    readPos = 0;                      // 读指针归零
    writePos = 0;                     // 写指针归零
}

// 获取所有可读数据并回收，返回字符串
string Buffer::retrieveAllToStr()
{
    string str(peek(), readableBytes()); // 拷贝所有可读数据到字符串
    retrieveAll();                       // 回收所有数据
    return str;                          // 返回字符串
}

// 获取可写区域的常量指针
const char* Buffer::beginWriteConst() const
{
    return beginPtr() + writePos;
}

// 获取可写区域的指针
char* Buffer::beginWrite()
{
    return beginPtr() + writePos;
}

// 写入数据后，更新写指针
void Buffer::hasWritten(size_t len)
{
    writePos += len;
}

// 追加字符串数据
void Buffer::append(const string& str)
{
    append(str.data(), str.length());
}

// 追加任意数据块
void Buffer::append(const void* data, size_t len)
{
    assert(data); // 检查数据指针合法
    append(static_cast<const char*>(data), len); // 转为char*后追加
}

// 追加字符数组数据
void Buffer::append(const char* str, size_t len)
{
    assert(str);           // 检查数据指针合法
    ensureWritable(len);   // 确保有足够可写空间
    copy(str, str + len, beginWrite()); // 拷贝数据到缓冲区
    hasWritten(len);       // 更新写指针
}

// 追加另一个缓冲区的数据
void Buffer::append(const Buffer& buffer)
{
    append(buffer.peek(), buffer.readableBytes());
}

// 确保有足够可写空间，不够则扩容
void Buffer::ensureWritable(size_t len)
{
    if (writableBytes() < len)
    {
        makeSpace(len); // 扩容或移动数据
    }
    assert(writableBytes() >= len); // 扩容后再次检查
}

// 从文件描述符读取数据到缓冲区
ssize_t Buffer::readfd(int fd, int* saveErrno)
{
    char newbuffer[65536]; // 临时缓冲区
    struct iovec iov[2];   // 分散读结构体
    const size_t writable = writableBytes(); // 当前可写空间

    iov[0].iov_base = beginPtr() + writePos; // 第一块：主缓冲区可写部分
    iov[0].iov_len = writable;
    iov[1].iov_base = newbuffer;             // 第二块：临时缓冲区
    iov[1].iov_len = sizeof(newbuffer);

    const ssize_t len = readv(fd, iov, 2);   // 分散读，最多读两块
    if (len < 0)
    {
        *saveErrno = errno;                  // 读取失败，保存错误码
    }
    else if (static_cast<size_t>(len) <= writable)
    {
        writePos += len;                     // 读入数据未超出主缓冲区，直接更新写指针
    }
    // 缓冲区已满，剩余数据追加到主缓冲区
    else
    {
        writePos = buffer.size();            // 主缓冲区写满
        append(newbuffer, len - writable);   // 剩余数据追加到主缓冲区
    }
    return len;                              // 返回读取字节数
}

// 将缓冲区数据写入文件描述符
ssize_t Buffer::writefd(int fd, int* saveErrno)
{
    size_t readSize = readableBytes();       // 可读数据长度
    ssize_t len = write(fd, peek(), readSize); // 写入数据
    if (len < 0)
    {
        *saveErrno = errno;                  // 写入失败，保存错误码
        return len;
    }
    readPos += len;                          // 写入成功，回收已写数据
    return len;                              // 返回写入字节数
}

// 获取底层数据指针
char* Buffer::beginPtr()
{
    return &*buffer.begin();
}

// 获取底层数据常量指针
const char* Buffer::beginPtr() const
{
    return &*buffer.begin();
}

// 扩展缓冲区空间或移动数据
void Buffer::makeSpace(size_t len)
{
    // 如果总可用空间不足（可写+前置空间），则扩容
    if (writableBytes() + prependableBytes() < len)
    {
        buffer.resize(writePos + len + 1); // 扩容到足够大小
    }
    // 否则，将未读数据移动到缓冲区前端
    else
    {
        size_t readable = readableBytes(); // 未读数据长度
        // 将未读数据移动到缓冲区起始位置
        copy(beginPtr() + readPos, beginPtr() + writePos, beginPtr());
        readPos = 0;                       // 读指针归零
        writePos = readPos + readable;     // 写指针指向未读数据末尾
        assert(readable == readableBytes());// 检查移动后数据长度不变
    }
}