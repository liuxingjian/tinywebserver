/**
 * @file httpresponse.cpp
 * @brief HttpResponse类的实现，用于在简单Web服务器中处理HTTP响应。
 *
 * 本文件定义了构建HTTP响应报文的逻辑，包括状态行、响应头和内容体。
 * 支持文件映射以高效地提供文件，并通过自定义HTML页面处理错误响应。
 * 该类维护了文件后缀到MIME类型、HTTP状态码到消息、错误码到错误页面路径的映射。
 *
 * 主要功能：
 * - 响应对象和文件映射的初始化与清理。
 * - 根据文件是否存在、权限和请求路径构建HTTP响应报文。
 * - 针对常见HTTP状态码（400、403、404）进行错误处理并返回自定义错误页面。
 * - 根据文件后缀判断MIME类型。
 * - 支持keep-alive长连接。
 *
 * 依赖：
 * - Buffer类用于构建响应报文。
 * - POSIX文件操作和内存映射（mmap, munmap）。
 *
 * 用法：
 * - 创建HttpResponse对象，使用请求参数初始化，并调用makeResponse()生成HTTP响应。
 */
#include "httpresponse.h"

using namespace std;
// 文件后缀与MIME类型的映射表
const unordered_map<string, string> HttpResponse::SUFFIX_TYPE =
{
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/msword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

// HTTP状态码与状态消息的映射表
const unordered_map<int, string> HttpResponse::CODE_STATUS =
{
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

// 错误码与错误页面路径的映射表
const unordered_map<int, string> HttpResponse::CODE_PATH =
{
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

// 构造函数，初始化成员变量
HttpResponse::HttpResponse()
{
    code = -1;                // 初始化状态码为-1
    path = srcDir = "";       // 初始化路径和资源目录为空字符串
    isKeepAlive = false;      // 初始化长连接标志为false
    mmFile = nullptr;         // 初始化文件映射指针为空
    mmFileStat = {0};         // 初始化文件状态结构体
}

// 析构函数，解除文件映射
HttpResponse::~HttpResponse()
{
    unmapFile();              // 析构时解除文件映射
}

/* 响应报文初始化： 状态码，长连接，文件路径 */
void HttpResponse::init(const string& srcDir, string& path, bool isKeepAlive, int code)
{
    assert(srcDir != "");     // 资源目录不能为空
    if (mmFile) unmapFile();  // 如果已映射文件，先解除映射
    this->code = code;        // 设置状态码
    this->isKeepAlive = isKeepAlive; // 设置长连接标志
    this->path = path;        // 设置请求路径
    this->srcDir = srcDir;    // 设置资源目录
    mmFile = nullptr;         // 文件映射指针置空
    mmFileStat = {0};         // 文件状态结构体初始化
}

/* 制作响应报文 */
void HttpResponse::makeResponse(Buffer& buffer)
{
    // 检查文件是否存在或是否为目录
    if (stat((srcDir + path).data(), &mmFileStat) < 0 || S_ISDIR(mmFileStat.st_mode))
    {
        code = 404;           // 文件不存在或为目录，返回404
    }
    else if (!(mmFileStat.st_mode & S_IROTH))
    {
        code = 403;           // 没有其他用户的读权限，返回403
    }
    else if (code == -1)
    {
        code = 200;           // 正常情况，返回200
    }
    errorHtml();              // 如果是错误码，跳转到错误页面
    addState(buffer);         // 添加状态行
    addHeader(buffer);        // 添加响应头
    addContent(buffer);       // 添加响应体
}

/* 获取映射好的文件指针 */
char* HttpResponse::getFile()
{
    return mmFile;            // 返回文件映射指针
}

// 获取文件长度
size_t HttpResponse::getFileLen() const
{
    return mmFileStat.st_size; // 返回文件长度
}

/* 范围外错误页面，直接生成错误内容 */
void HttpResponse::errorContent(Buffer& buffer, string message)
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(code))
    {
        status = CODE_STATUS.find(code)->second; // 获取状态消息
    }
    else
    {
        status = "Bad Request";                  // 未知状态码，默认Bad Request
    }
    body += to_string(code) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>SimpleWebServer</em></body></html>";

    buffer.append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buffer.append(body);      // 添加错误内容到缓冲区
}

// 获取当前状态码
int HttpResponse::getCode() const
{
    return code;              // 返回当前状态码
}

/* 解除文件映射 */
void HttpResponse::unmapFile()
{
    if (mmFile)
    {
        munmap(mmFile, mmFileStat.st_size); // 解除映射
        mmFile = nullptr;                   // 指针置空
    }
}

/* 添加状态行 */
void HttpResponse::addState(Buffer& buffer)
{
    string status;
    if (CODE_STATUS.count(code))
    {
        status = CODE_STATUS.find(code)->second; // 获取状态消息
    }
    else
    {
        code = 400;                             // 未知状态码，默认400
        status = CODE_STATUS.find(400)->second;
    }
    buffer.append("HTTP/1.1 " + to_string(code) + " " + status + "\r\n");
}

/* 添加响应头 */
void HttpResponse::addHeader(Buffer& buffer)
{
    buffer.append("Connection: ");
    if (isKeepAlive)
    {
        buffer.append("keep-alive\r\n");        // 长连接
        buffer.append("keep-alive: max=6, timeout=120\r\n");
    }
    else
    {
        buffer.append("close\r\n");             // 非长连接
    }
    buffer.append("Content-type: " + getFileType() + "\r\n"); // 添加内容类型
}

/* 添加响应体 */
void HttpResponse::addContent(Buffer& buffer)
{
    int srcfd = open((srcDir + path).data(), O_RDONLY); // 打开文件
    if (srcfd < 0)
    {
        errorContent(buffer, "File Not Found!"); // 打开失败，返回错误内容
        return;
    }
    LOG_DEBUG("file path %s", (srcDir + path).data());

    // 将文件映射到内存，提高访问速度
    // PROT_READ：映射区可读
    // MAP_PRIVATE：写入时复制
    int* mmRet = (int*)mmap(0, mmFileStat.st_size, PROT_READ, MAP_PRIVATE, srcfd, 0);
    if (*mmRet == -1)
    {
        errorContent(buffer, "File Not Found!"); // 映射失败，返回错误内容
        return;
    }
    mmFile = (char*)mmRet;                      // 保存映射指针
    close(srcfd);                               // 关闭文件描述符
    buffer.append("Content-length: " + to_string(mmFileStat.st_size) + "\r\n\r\n");
}

/* 范围内的错误页面，设置错误页面路径 */
void HttpResponse::errorHtml()
{
    if (CODE_PATH.count(code))
    {
        path = CODE_PATH.find(code)->second;    // 设置错误页面路径
        stat((srcDir + path).data(), &mmFileStat); // 获取错误页面文件状态
    }
}

/* 判断文件类型，返回MIME类型 */
string HttpResponse::getFileType()
{
    string::size_type idx = path.find_last_of('.');
    if (idx == string::npos) return "text/plain"; // 没有后缀，返回默认类型
    string suffix = path.substr(idx);
    if (SUFFIX_TYPE.count(suffix))
    {
        return SUFFIX_TYPE.find(suffix)->second; // 返回对应MIME类型
    }
    return "text/plain";                         // 未知后缀，返回默认类型
}
