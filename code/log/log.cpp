/**
 * @file log.cpp
 * @brief 线程安全日志系统实现，支持同步和异步模式。
 *
 * 本日志系统主要功能：
 * - 同步日志：线程直接写入日志文件，日志写入与业务逻辑串行执行。
 * - 异步日志：线程将日志消息加入阻塞队列，由专用线程写入日志文件。
 * - 文件访问加锁，防止多线程并发写入导致日志混乱。
 * - 单例模式管理阻塞队列和写线程（异步模式）。
 * - 日志文件自动按日期或行数轮转。
 * - 每次写入或轮转前立即刷新日志缓冲区。
 *
 * 
 * 主要组件：
 * - Log类：管理日志写入、文件轮转和线程同步。
 * - 阻塞队列：异步模式下缓存日志消息。
 * - 写线程：专用线程将队列中的日志写入文件。
 * - 互斥锁：保证日志文件和缓冲区的线程安全。
 *
 * 使用方法：
 * - 初始化logger，设置日志等级、文件路径、后缀和队列大小。
 * - 使用write()方法按指定等级和格式写日志。
 * - 支持日志等级：debug、info、warn、error。
 * - 日志文件每日或超过最大行数自动轮转。
 * - 使用flush()强制立即写入缓冲日志。
 *
 * 线程安全：
 * - 所有公有方法均为线程安全。
 * - 异步模式下日志写入高效，竞争最小。
 *
 * @author
 * @date
 */
/* 同步/异步写日志
同步写日志：线程直接向文件内写入日志，写日志与线程业务是串行的
异步写日志：线程先将日志放到阻塞队列中，再用专门的线程向文件内写日志

为了避免日志混乱，需要用互斥锁实现文件的互斥访问，写日志前需要上锁
对于异步写日志：用单例模式维护一个阻塞队列，一个写线程，节约资源，减少竞态

每次调用写日志方法后，或创建新日志文件前（日期改变，行数超限），都要调用flush将缓冲区内数据全部写入文件中
如果是同步写日志，fflush(fp)即可，如果是异步，还要先保证阻塞队列被清空

*/

// 包含头文件，声明 Log 类和相关依赖（Buffer、BlockQueue、常量等）
#include "log.h"

using namespace std;

// 构造函数：将成员初始化为安全的默认值，避免野指针和未定义行为
Log::Log() {
    lineCount = 0;        // 当前文件已写日志行数
    isAsync = false;      // 默认同步模式，异步需要在 init 中启用
    writeThread = nullptr;// 写线程的智能指针（unique_ptr）初始为空
    que = nullptr;        // 阻塞队列指针初始为空
    today = 0;            // 记录当前日志文件对应的天数
    fp = nullptr;         // FILE* 初始为空，后续在 init 打开
}

// 析构函数：清理写线程与文件指针，保证退出前将队列中的日志写完
Log::~Log() {
    // 如果存在写线程并且可 join，说明处于异步模式并且线程仍在运行
    if(writeThread && writeThread->joinable()) {
        // 循环确保队列里日志被处理完
        // 注意：这里依赖 BlockQueue 的实现，可能会引入 busy-loop
        while(!que->empty())
        {
            que->flush(); // 通知写线程尽快消费（语义由 BlockQueue 决定）
        };
        que->close();
        // 等待写线程退出并回收资源
        writeThread->join();
    }
    // 如果文件指针存在，先上锁，flush 缓冲并关闭文件
    if(fp) {
        lock_guard<mutex> locker(mtx);
        flush();
        fclose(fp);
    }
}

// 获取日志级别（线程安全）
int Log::getLevel() {
    lock_guard<mutex> locker(mtx);
    return level_;
}

// 设置日志级别（线程安全）
void Log::setLevel(int level) {
    lock_guard<mutex> locker(mtx);
    level_ = level;
}

/* 初始化：配置日志等级、路径、后缀、是否启用异步和写线程 */
void Log::init(int level = 1, const char* path, const char* suffix,
    int maxQueueSize) {
    isOpen_ = true;      // 标记日志系统已打开
    level_ = level;      // 设置日志等级
    // 如果指定了队列大小大于 0，则启用异步
    if(maxQueueSize > 0)
    {
        isAsync = true;
        if(!que)
        {
            // 使用 unique_ptr 管理阻塞队列对象
            unique_ptr<BlockQueue<std::string>> newQueue(new BlockQueue<std::string>);
            que = move(newQueue);
            
            // 启动写线程，线程回调是静态函数 flushLogThread
            std::unique_ptr<std::thread> NewThread(new thread(flushLogThread));
            writeThread = move(NewThread);
        }
    } 
    else 
    {
        // 不使用异步模式，写操作由调用线程完成
        isAsync = false;
    }

    lineCount = 0; // 初始时已写行数为 0

    // 初始化日志文件名：基于当前日期构造路径/年_月_日后缀
    time_t timer = time(nullptr);
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;
    path_ = path;     // 保存日志目录
    suffix_ = suffix; // 保存日志文件后缀
    char fileName[LOG_NAME_LEN] = {0};
    // 格式化文件名，例如 path_/2025_10_10.log
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    today = t.tm_mday; // 记录今天的天

    // 初始化文件指针（在互斥锁保护下操作）
    {
        lock_guard<mutex> locker(mtx);
        buffer.retrieveAll(); // 清空缓冲
        if(fp) { 
            flush();   // 如果已有打开的文件，先 flush 并关闭
            fclose(fp); 
        }

        // 以追加模式打开文件，若目录不存在则尝试创建目录
        fp = fopen(fileName, "a");
        if(fp == nullptr) {
            mkdir(path_, 0777);
            fp = fopen(fileName, "a");
        } 
        assert(fp != nullptr); // 确保打开成功（调试时有用，生产应更稳健处理）
    }
}

/* 写日志：支持按日期或按行数轮转、支持同步与异步两种模式 */
void Log::write(int level, const char *format, ...) {
    // 获取当前时间（带微秒）并转换为本地时间结构
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    // 判断是否需要轮转：日期变了或行数达到阈值（MAX_LINES 的倍数）
    if (today != t.tm_mday || (lineCount && (lineCount % MAX_LINES) == 0))
    {
        // 使用 unique_lock 以便我们可以显式 unlock/lock（控制临界区）
        unique_lock<mutex> locker(mtx);
        locker.unlock(); // 先释放锁，缩短持锁时间（注意竞态风险）
        
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        // 构造日期后缀，例如 2025_10_10
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (today != t.tm_mday)
        {
            // 如果是新的一天，则用新的日期作为文件名
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            today = t.tm_mday;
            lineCount = 0; // 新文件行数从 0 开始
        }
        else {
            // 按行数轮转，文件名带上轮转序号（lineCount / MAX_LINES）
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount  / MAX_LINES), suffix_);
        }
        
        // 再次加锁以安全替换文件指针
        locker.lock();
        flush(); // 确保先前的缓冲/队列内容写完
        fclose(fp);
        fp = fopen(newFile, "a");
        assert(fp != nullptr);
    }

    // 将日志内容写入 buffer（并视情况直接写入文件或推送到队列）
    {
        unique_lock<mutex> locker(mtx); // 持锁保护 buffer 与 fp
        lineCount++;
        // 写入时间戳到 buffer 的可写区域（假设至少有 128 字节可写）
        int n = snprintf(buffer.beginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
                    
        buffer.hasWritten(n); // 更新 buffer 写入位置
        appendLogLevelTitle(level); // 在 buffer 中追加等级前缀

        // 处理变参格式化，将用户提供的格式字符串写入 buffer
        va_start(vaList, format);
        int m = vsnprintf(buffer.beginWrite(), buffer.writableBytes(), format, vaList);
        va_end(vaList);

        buffer.hasWritten(m); // 标记写入了 m 字节（注意：若 m >= writableBytes() 则会有截断风险）
        buffer.append("\n\0", 2); // 追加换行和结束符，便于用 C 字符串输出

        if(isAsync && que && !que->full()) {
            // 异步模式：将当前 buffer 的字符串取出并 push 到队列，由写线程负责写入文件
            que->push(buffer.retrieveAllToStr());
        } else {
            // 同步模式：直接把 buffer 的内容写入文件
            fputs(buffer.peek(), fp);
        }
        buffer.retrieveAll(); // 清空 buffer 写区
    }
}

/* 添加日志等级信息到 buffer 中（如 [info]、[error]） */
void Log::appendLogLevelTitle(int level) {
    switch(level) {
    case 0:
        buffer.append("[debug]: ", 9);
        break;
    case 1:
        buffer.append("[info] : ", 9);
        break;
    case 2:
        buffer.append("[warn] : ", 9);
        break;
    case 3:
        buffer.append("[error]: ", 9);
        break;
    default:
        buffer.append("[info] : ", 9);
        break;
    }
}

/* 将缓存数据立即写入文件中 */
void Log::flush() {
    // 如果启用了异步，将队列 flush（通常会通知写线程尽快写入）
    if(isAsync) 
    { 
        que->flush(); 
    }
    // 将 FILE* 缓冲写入磁盘（C 标准库缓冲）
    fflush(fp);
}

/* 异步写入线程主循环：不断从队列 pop 并写入文件 */
void Log::asyncWrite() {
    string str = "";
    // 当 que->pop 返回 false 时表示队列已关闭且空，线程应退出
    while(que->pop(str)) {
        lock_guard<mutex> locker(mtx); // 写文件时也需要互斥，避免与轮转冲突
        fputs(str.c_str(), fp);
    }
}

/* 单例模式：返回唯一 Log 实例（C++11 局部静态线程安全） */
Log* Log::instance() {
    static Log obj;
    return &obj;
}

/* 写线程回调函数：线程入口，调用单例的 asyncWrite */
void Log::flushLogThread() {
    Log::instance()->asyncWrite();
}