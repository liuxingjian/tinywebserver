/**
 * @class HttpResponse
 * @brief 用于简单Web服务器的HTTP响应构建和文件映射处理。
 *
 * 该类负责生成HTTP响应，包括状态行、头部和内容。支持使用内存映射方式提供静态文件、错误处理和长连接管理。
 *
 * @note 使用Buffer进行响应构建，并集成日志工具。
 *
 * @method HttpResponse() 构造函数。
 * @method ~HttpResponse() 析构函数。
 * @method void init(const string& srcDir, string& path, bool isKeepAlive = false, int code = -1)
 *         初始化响应，包括源目录、文件路径、长连接标志和状态码。
 * @method void makeResponse(Buffer& buffer)
 *         构建完整HTTP响应并写入Buffer。
 * @method char* getFile()
 *         返回映射文件内容的指针。
 * @method size_t getFileLen() const
 *         返回映射文件的长度。
 * @method void errorContent(Buffer& buffer, string message)
 *         生成带有指定消息的错误响应。
 * @method int getCode() const
 *         返回HTTP状态码。
 * @method void unmapFile()
 *         解除文件的内存映射。
 *
 * @private
 * @method void addState(Buffer &buffer)
 *         添加HTTP状态行到Buffer。
 * @method void addHeader(Buffer &buffer)
 *         添加HTTP头部到Buffer。
 * @method void addContent(Buffer &buffer)
 *         添加响应内容到Buffer。
 * @method void errorHtml()
 *         准备错误页面HTML内容。
 * @method string getFileType()
 *         获取请求文件的MIME类型。
 *
 * @member int code HTTP状态码。
 * @member bool isKeepAlive 长连接标志。
 * @member string path 请求的文件路径。
 * @member string srcDir 文件源目录。
 * @member char* mmFile 内存映射文件指针。
 * @member struct stat mmFileStat 文件统计信息。
 * @member static const unordered_map<string, string> SUFFIX_TYPE 文件后缀到MIME类型的映射。
 * @member static const unordered_map<int, string> CODE_STATUS 状态码到状态消息的映射。
 * @member static const unordered_map<int, string> CODE_PATH 错误码到错误页面路径的映射。
 */
#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "../buffer/buffer.h"
#include "../log/log.h"

using namespace std;

class HttpResponse
{
public:
    HttpResponse(); // 构造函数，初始化HttpResponse对象
    ~HttpResponse(); // 析构函数，释放资源

    // 初始化响应，包括源目录、文件路径、长连接标志和状态码
    void init(const string& srcDir, string& path, bool isKeepAlive = false, int code = -1);

    // 构建完整HTTP响应并写入Buffer
    void makeResponse(Buffer& buffer);

    // 返回映射文件内容的指针
    char* getFile();

    // 返回映射文件的长度
    size_t getFileLen() const;

    // 生成带有指定消息的错误响应
    void errorContent(Buffer& buffer, string message);

    // 返回HTTP状态码
    int getCode() const;

    // 解除文件的内存映射
    void unmapFile();

private:
    // 添加HTTP状态行到Buffer
    void addState(Buffer &buffer);

    // 添加HTTP头部到Buffer
    void addHeader(Buffer &buffer);

    // 添加响应内容到Buffer
    void addContent(Buffer &buffer);

    // 准备错误页面HTML内容
    void errorHtml();

    // 获取请求文件的MIME类型
    string getFileType();

    int code; // HTTP状态码
    bool isKeepAlive; // 长连接标志

    string path; // 请求的文件路径
    string srcDir; // 文件源目录

    char* mmFile; // 内存映射文件指针
    struct stat mmFileStat; // 文件统计信息

    // 文件后缀到MIME类型的映射
    static const unordered_map<string, string> SUFFIX_TYPE;

    // 状态码到状态消息的映射
    static const unordered_map<int, string> CODE_STATUS;

    // 错误码到错误页面路径的映射
    static const unordered_map<int, string> CODE_PATH;
};

#endif