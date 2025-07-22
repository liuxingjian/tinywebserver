#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>

class MySQLConnectionPool {
public:
    static MySQLConnectionPool* get_instance();

    void init(const std::string& host, int port,
              const std::string& user, const std::string& password,
              const std::string& db_name, int max_conn);

    MYSQL* get_connection();
    void release_connection(MYSQL* conn);

    ~MySQLConnectionPool();

private:
    MySQLConnectionPool() = default;

    std::queue<MYSQL*> conn_queue_;
    std::mutex mtx_;
    std::condition_variable cond_;
    int max_conn_;
    int used_conn_;
};

class MySQLRAII {
public:
    MySQLRAII(MYSQL** conn, MySQLConnectionPool* pool);
    ~MySQLRAII();

private:
    MYSQL* conn_;
    MySQLConnectionPool* pool_;
};

#endif
