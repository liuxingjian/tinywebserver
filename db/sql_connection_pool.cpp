#include "sql_connection_pool.h"
#include <iostream>

MySQLConnectionPool* MySQLConnectionPool::get_instance() {
    static MySQLConnectionPool instance;
    return &instance;
}

void MySQLConnectionPool::init(const std::string& host, int port,
                               const std::string& user, const std::string& password,
                               const std::string& db_name, int max_conn) {
    std::lock_guard<std::mutex> lock(mtx_);
    for (int i = 0; i < max_conn; ++i) {
        MYSQL* conn = mysql_init(nullptr);
        if (!conn) {
            std::cerr << "MySQL init failed\n";
            exit(1);
        }

        conn = mysql_real_connect(conn, host.c_str(), user.c_str(),
                                  password.c_str(), db_name.c_str(), port,
                                  nullptr, 0);
        if (!conn) {
            std::cerr << "MySQL connect error: " << mysql_error(conn) << "\n";
            exit(1);
        }

        conn_queue_.push(conn);
    }

    max_conn_ = max_conn;
    used_conn_ = 0;
}

MYSQL* MySQLConnectionPool::get_connection() {
    std::unique_lock<std::mutex> lock(mtx_);
    while (conn_queue_.empty()) {
        cond_.wait(lock);
    }

    MYSQL* conn = conn_queue_.front();
    conn_queue_.pop();
    ++used_conn_;
    return conn;
}

void MySQLConnectionPool::release_connection(MYSQL* conn) {
    std::lock_guard<std::mutex> lock(mtx_);
    conn_queue_.push(conn);
    --used_conn_;
    cond_.notify_one();
}

MySQLConnectionPool::~MySQLConnectionPool() {
    std::lock_guard<std::mutex> lock(mtx_);
    while (!conn_queue_.empty()) {
        MYSQL* conn = conn_queue_.front();
        conn_queue_.pop();
        mysql_close(conn);
    }
}

MySQLRAII::MySQLRAII(MYSQL** conn, MySQLConnectionPool* pool) {
    *conn = pool->get_connection();
    conn_ = *conn;
    pool_ = pool;
}

MySQLRAII::~MySQLRAII() {
    pool_->release_connection(conn_);
}
