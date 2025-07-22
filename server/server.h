#ifndef SERVER_H
#define SERVER_H
#include "../threadpool/threadpool.h"
#include "../timer/timer.h"
class Server{
    public:
    Server(int port,int thread_num);
    ~Server();
    void run();
    private:
    int listen_fd;
    int epoll_fd;
    int port;
    ThreadPool thread_pool;
    void handle_connection(int conn_fd);
    void handle_request(int client_fd);
    void set_nonblocking(int fd);
    TimerManager timer_manager;
    const int timeout_ms = 5000;  // 5秒超时

};
#endif
