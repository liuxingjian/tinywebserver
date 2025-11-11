/**
 * @file sql_conn_pool.cpp
 * @brief MySQL连接池的实现，用于高效数据库访问。
 *
 * 本文件定义了SqlConnPool类，用于管理MySQL连接池，
 * 支持多线程高效获取和释放连接。同时定义了SqlConnect RAII封装类，
 * 实现自动连接管理。
 *
 * 类说明:
 * - SqlConnPool: 单例类，负责初始化、管理和销毁MySQL连接池。
 *   - init: 初始化连接池。
 *   - destroy: 清理所有连接和资源。
 *   - instance: 获取连接池单例。
 *   - getConn: 从池中获取连接。
 *   - releaseConn: 释放连接回池。
 *   - getFreeConnCnt: 获取空闲连接数。
 *   - ~SqlConnPool: 析构函数，调用destroy()。
 *
 * - SqlConnect: MySQL连接的RAII封装。
 *   - SqlConnect: 从池中获取连接。
 *   - ~SqlConnect: 归还连接到池。
 *
 * 线程安全:
 * - 使用互斥锁和信号量保证连接队列的并发安全。
 *
 * 依赖:
 * - MySQL C API
 * - 自定义日志宏（LOG_ERROR）
 * - 自定义同步原语（mtx, sem）
 */
#include "sql_conn_pool.h"

using namespace std;                          // 使用 std 命名空间，简化类型书写

SqlConnPool::SqlConnPool()
{
    useConnCnt = 0;                           // 已使用连接计数初始化为0
    freeConnCnt = 0;                          // 空闲连接计数初始化为0
}

void SqlConnPool::init(
    const char* host, int port, 
    const char* user, const char* pwd,
    const char* dbName, int maxConnCnt = 10)
{
    assert(maxConnCnt > 0);                   // 确保最大连接数大于0
    for (int i = 0; i < maxConnCnt; i ++)
    {
        MYSQL *sql = nullptr;                 // 临时指针，表示单个 MySQL 连接
        sql = mysql_init(sql);                // 初始化 MYSQL 结构
        if (!sql)
        {
            LOG_ERROR("MySQL init error!");   // 记录初始化错误
            assert(sql);                      // 断言失败（程序中止）以便调试
        }
        sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0); // 连接数据库
        if (!sql)
        {
            LOG_ERROR("MySQL Connect error"); // 连接失败记录错误（但不立即中止）
        }
        connQue.push(sql);                    // 将连接加入队列（作为空闲连接）
    }
    this->freeConnCnt = maxConnCnt;           // 设置空闲连接计数为最大连接数

    mtxPool = new mtx();                      // 创建互斥锁对象，用于保护连接队列
    semFree = new sem(maxConnCnt);            // 创建一个计数信号量，初始值为最大连接数
}

void SqlConnPool::destroy()
{
    this->mtxPool->lock();                    // 加锁，开始销毁连接过程
    while (!connQue.empty())
    {
        MYSQL* item = connQue.front();        // 取出队首连接
        connQue.pop();                        // 从队列中移除
        mysql_close(item);                    // 关闭 MySQL 连接
    }
    this->mtxPool->unlock();                  // 解锁
    delete(mtxPool);                          // 释放互斥锁对象内存
    delete(semFree);                          // 释放信号量对象内存
    mysql_library_end();                      // 释放 MySQL 客户端库资源
}

SqlConnPool* SqlConnPool::instance()
{
    static SqlConnPool connPool;              // 局部静态单例对象（线程安全）
    return &connPool;                         // 返回单例实例指针
}

MYSQL* SqlConnPool::getConn()
{
    MYSQL *sql = nullptr;

    this->semFree->wait();                    // 等待（或减少）信号量，确保有空闲连接
    this->mtxPool->lock();                    // 加锁保护队列访问
    sql = connQue.front();                    // 从队列获取一个连接
    connQue.pop();                            // 将其从队列移除（成为使用中）
    this->useConnCnt++;                       // 已使用连接计数增加
    this->freeConnCnt--;                      // 空闲连接计数减少
    this->mtxPool->unlock();                  // 解锁
    
    return sql;                               // 返回获取到的连接
}

void SqlConnPool::releaseConn(MYSQL *sql)
{
    assert(sql);                              // 确保传入连接非空
    this->mtxPool->lock();                    // 加锁保护队列访问
    connQue.push(sql);                        // 将连接放回队列（变为空闲）
    this->useConnCnt--;                       // 已使用连接计数减少
    this->freeConnCnt++;                      // 空闲连接计数增加
    this->mtxPool->unlock();                  // 解锁
    this->semFree->post();                    // 释放（增加）信号量，通知有可用连接
}

int SqlConnPool::getFreeConnCnt()
{
    this->mtxPool->lock();                    // 加锁以读取队列状态
    return connQue.size();                    // 返回队列中空闲连接数量
    this->mtxPool->unlock();                  // （注意：此行在 return 后不可达，应放在 return 之前）
}

SqlConnPool::~SqlConnPool()
{
    this->destroy();                          // 析构时销毁连接池并释放资源
}

SqlConnect::SqlConnect(MYSQL** psql, SqlConnPool *sqlConnPool)
{
    assert(sqlConnPool);                      // 确保传入连接池非空
    this->sql = sqlConnPool->getConn();       // 从连接池获取连接
    *psql = this->sql;                        // 将连接指针返回给调用者
    this->sqlConnPool = sqlConnPool;          // 保存连接池指针以便析构时释放连接
}

SqlConnect::~SqlConnect()
{
    if (this->sql)
    {
        this->sqlConnPool->releaseConn(this->sql); // RAII：析构时将连接归还给连接池
    }
}