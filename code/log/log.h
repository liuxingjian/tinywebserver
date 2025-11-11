#ifndef LOG_H
#define LOG_H

#include <mutex>           // 互斥锁，用于线程安全
#include <string>          // 字符串类型
#include <thread>          // 线程相关
#include <sys/time.h>      // 时间相关
#include <string.h>        // 字符串操作
#include <stdarg.h>        // 可变参数处理（va_start, va_end）
#include <assert.h>        // 断言
#include <sys/stat.h>      // 文件/目录操作（mkdir）
#include "blockqueue.h"    // 异步日志队列
#include "../buffer/buffer.h" // 缓冲区类

// 日志类定义
class Log {
public:
    // 初始化日志系统，设置日志等级、路径、后缀、异步队列最大长度
    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    // 获取单例对象
    static Log* instance();
    // 异步日志线程入口
    static void flushLogThread();

    // 写日志（支持可变参数格式化）
    void write(int level, const char *format,...);
    // 刷新日志（立即写入文件）
    void flush();

    // 获取当前日志等级
    int getLevel();
    // 设置日志等级
    void setLevel(int level);
    // 判断日志系统是否打开
    bool isOpen() { return isOpen_; }
    
private:
    Log(); // 构造函数（私有，单例模式）
    // 追加日志等级头部（如[INFO]）
    void appendLogLevelTitle(int level);
    virtual ~Log(); // 析构函数
    // 异步写日志实现
    void asyncWrite();

private:
    static const int LOG_PATH_LEN = 256; // 日志路径最大长度
    static const int LOG_NAME_LEN = 256; // 日志文件名最大长度
    static const int MAX_LINES = 50000;  // 单个日志文件最大行数

    const char* path_;   // 日志文件路径
    const char* suffix_; // 日志文件后缀

    int MAX_LINES_;      // 当前日志文件最大行数

    int lineCount;       // 当前日志文件已写行数
    int today;           // 当前日期（用于日志分割）

    bool isOpen_;        // 日志系统是否打开
 
    Buffer buffer;       // 日志缓冲区
    int level_;          // 当前日志等级
    bool isAsync;        // 是否异步写日志

    FILE* fp;            // 日志文件指针
    std::unique_ptr<BlockQueue<std::string>> que; // 异步日志队列
    std::unique_ptr<std::thread> writeThread;     // 异步写线程
    std::mutex mtx;      // 互斥锁，保证线程安全
};

// 日志基础宏，level为日志等级，format为格式化字符串，__VA_ARGS__为可变参数
// 调用日志写入，并立即刷新到文件
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::instance();\
        if (log->isOpen() && log->getLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

// 定义四个日志等级宏，方便在其他文件中直接调用
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H