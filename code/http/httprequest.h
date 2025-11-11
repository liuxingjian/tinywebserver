/**
 * @file httpresponse.h
 * @brief 定义用于解析和处理 HTTP 请求的 HttpRequest 类。
 *
 * 此头文件提供了 HttpRequest 类，负责从缓冲区解析 HTTP 请求行、请求头和请求体，
 * 并管理请求状态，提取方法、路径、版本和请求头等相关信息。同时包含用户验证功能
 * 及处理 URL 编码数据的工具函数。
 *
 * 枚举类型:
 * - HTTP_CODE: 表示 HTTP 请求解析和处理的结果。
 * - PARSE_STATE: 表示 HTTP 请求解析过程的当前状态。
 *
 * 类:
 * - HttpRequest: 负责解析和管理 HTTP 请求。
 *
 * 依赖:
 * - 标准 C++ 库: <unordered_map>, <unordered_set>, <string>, <regex>
 * - MySQL 客户端库: <mysql/mysql.h>
 * - 项目相关模块: buffer, log, sql_conn_pool, threadpool
 */
#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>
#include <mysql/mysql.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../sqlconnpool/sql_conn_pool.h"
#include "../threadpool/threadpool.h"

using namespace std;

enum HTTP_CODE
{
    NO_REQUEST = 0,
    // 表示当前没有完整的 HTTP 请求，需要继续读取数据
    GET_REQUEST,
    // 表示成功解析到一个 GET 请求（或一般的成功请求解析）
    BAD_REQUEST,
    // 表示客户端请求有语法错误，无法解析（400）
    NO_RESOURSE,
    // 表示请求的资源不存在（404），注意拼写为 NO_RESOURSE（resource 拼写错误但为历史原因保留）
    FORBIDDENT_REQUEST,
    // 表示请求被禁止访问（403），拼写为 FORBIDDENT（Forbidden 的变体）
    FILE_REQUEST,
    // 表示请求的是一个静态文件，通常用于返回文件内容
    INTERNAL_ERROR,
    // 表示服务器内部错误（500）
    CLOSED_CONNECTION
    // 表示客户端已经关闭连接或连接不可用
};

enum PARSE_STATE
{
    REQUEST_LINE = 0,
    // 解析请求行（method path version）阶段
    HEADERS,
    // 解析请求头部（Header 字段）阶段
    BODY,
    // 解析请求体（例如 POST 的 body）阶段
    FINISH
    // 解析完成
};

class HttpRequest
{
public:
    HttpRequest() {init();}
    ~HttpRequest() = default;

    // 构造时调用 init 初始化成员
    void init();
    // 从 Buffer 中解析 HTTP 请求，返回解析结果（HTTP_CODE）
    HTTP_CODE parse(Buffer& buffer);

    // 访问器：返回请求路径（常量拷贝/引用）
    string getPathConst() const {return path;}
    string& getPath() {return path;}
    // 返回请求方法（GET/POST 等）
    string getMethod() const {return method;}
    // 返回 HTTP 版本（如 HTTP/1.1）
    string getVersion() const {return version;}
    // 是否保持长连接（Connection: keep-alive）
    bool isKeepAlive() const;

private:
 // 解析请求行、头部和体的辅助函数
    HTTP_CODE parseRequestLine(const string& line);
    HTTP_CODE parseHeader(const string& line);
    HTTP_CODE parseBody();
   

    // 解析路径（处理 query 参数、默认页面映射等）
    void parsePath();
    // 解析 application/x-www-form-urlencoded 格式的 body（POST 表单）
    void parseFromUrlEncoded();

    // 静态工具：验证用户（登录/注册），与数据库交互（在 .cpp 中实现）
    static bool userVerify(const string& name, const string& pwd, bool isLogin);

    // 当前解析状态（请求行/头部/体/完成）
    PARSE_STATE state;
    // 请求方法、路径、版本与请求体
    string method, path, version, body;
    // 是否保持长连接（keep-alive）
    bool linger;
    // Content-Length（用于解析 body）
    size_t contentLen;
    // 存储请求头字段（键值对）
    unordered_map<string, string> header;
    // 存储解析后的 POST 表单键值对
    unordered_map<string, string> post;

    // 静态常量：默认支持的 HTML 页面集合与标签映射（在 cpp 中定义）
    static const unordered_set<string> DEFAULT_HTML;
    static const unordered_map<string, int> DEFAULT_HTML_TAG;
    // 将单个十六进制字符转换为数值
    static int convertHex(char ch);
};

#endif