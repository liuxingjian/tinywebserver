/**
 * @file sql_conn_pool.h
 * @brief SQL连接池和RAII SQL连接包装类的声明。
 *
 * 本文件定义了SqlConnPool类，用于管理MySQL连接池，实现线程安全的连接获取与释放。
 * 同时定义了SqlConnect类，实现对单个MySQL连接的RAII管理。
 */

/**
 * @class SqlConnPool
 * @brief 管理MySQL连接池，实现高效复用。
 *
 * 提供线程安全的初始化、销毁和连接获取功能。
 * 连接通过队列管理，访问通过互斥锁和信号量同步。
 */

/**
 * @brief 初始化连接池。
 * @param host MySQL服务器地址。
 * @param port MySQL服务器端口。
 * @param user MySQL用户名。
 * @param pwd MySQL密码。
 * @param dbName 数据库名称。
 * @param maxConnCnt 最大连接数。
 */

/**
 * @brief 销毁连接池并释放所有资源。
 */

/**
 * @brief 获取连接池的单例实例。
 * @return 指向单例SqlConnPool实例的指针。
 */

/**
 * @brief 从连接池获取一个MySQL连接。
 * @return 指向MYSQL连接的指针，若不可用则返回nullptr。
 */

/**
 * @brief 释放MySQL连接回连接池。
 * @param conn 要释放的MYSQL连接指针。
 */

/**
 * @brief 获取连接池中可用连接数。
 * @return 可用连接数。
 */

/**
 * @class SqlConnect
 * @brief RAII包装类，自动获取和释放MySQL连接。
 *
 * 构造时从连接池获取连接，析构时自动归还连接。
 */

/**
 * @brief 构造函数，获取连接。
 * @param psql 用于接收连接的MYSQL指针指针。
 * @param sqlConnPool 连接池指针。
 */

/**
 * @brief 析构函数，释放连接回连接池。
 */
#ifndef SQL_CONN_POOL_H
#define SQL_CONN_POOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <thread>
#include "../lock/locker.h"
#include <mutex>
#include <semaphore.h>
#include "../log/log.h"

using namespace std;

class SqlConnPool
{
public:
    void init(const char* host, int port,
              const char* user, const char* pwd,
              const char* dbName, int maxConnCnt);
    
    void destroy();

    static SqlConnPool *instance();

    MYSQL *getConn();
    void releaseConn(MYSQL *conn);
    int getFreeConnCnt();

private:
    SqlConnPool();
    ~SqlConnPool();

    int useConnCnt;
    int freeConnCnt;

    queue<MYSQL*> connQue;
    mtx *mtxPool;
    sem *semFree;
};


class SqlConnect
{
public:
    SqlConnect(MYSQL** psql, SqlConnPool *sqlConnPool);
    ~SqlConnect();

private:
    MYSQL *sql;
    SqlConnPool *sqlConnPool;
};

#endif