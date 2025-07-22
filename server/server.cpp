#include "server.h"         // 引入自定义服务器类头文件
#include <arpa/inet.h>      // 提供inet_addr等网络地址转换函数
#include <cstring>          // 提供memset、strlen等字符串处理函数
#include <unistd.h>         // 提供Unix标准系统调用(close、read、write等)
#include <iostream>         // 提供C++输入输出流
#include <fcntl.h>
#include <sys/epoll.h>
#include "../http/http_response.h"
#include "../http/http_request.h"


#define MAX_EVENTS 1024
#define BUFFER_SIZE 4096    // 定义接收缓冲区大小为4KB

// 构造函数：初始化端口号和监听套接字(-1表示未初始化)
Server::Server(int port,int thread_num) : 
port(port), thread_pool(thread_num)
 {
    listen_fd = socket(AF_INET, SOCK_STREAM,0);
    if(listen_fd < 0){
        perror("socket");
        exit(1);
    }
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;          // IPv4协议族
    server_addr.sin_port = htons(port);        // 端口号(转换为网络字节序)
    server_addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有可用网络接口
    int opt=1;
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(bind(listen_fd,(sockaddr*)&server_addr,sizeof(server_addr)) < 0){
        perror("bind");
        exit(1);
    }
    if(listen(listen_fd,5) < 0){
        perror("listen");
        exit(1);
    }
    epoll_fd = epoll_create1(0);
    if(epoll_fd < 0){
        perror("epoll_create1");
        exit(1);
    }
    set_nonblocking(listen_fd);
    epoll_event event{};
    event.data.fd = listen_fd;
    event.events = EPOLLIN;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_fd,&event) < 0){
        perror("epoll_ctl");
        exit(1);
    }

 }

// 析构函数：确保关闭监听套接字，防止资源泄漏
Server::~Server() {
    if (listen_fd != -1)
        close(listen_fd);
        close(epoll_fd);
}
void Server::set_nonblocking(int fd){
    int flags = fcntl(fd,F_GETFL,0);
    
    if(fcntl(fd,F_SETFL,flags | O_NONBLOCK) < 0){
        perror("fcntl");
        exit(1);
    }
}




void Server::run() {
    // 添加 timerfd 到 epoll
    epoll_event timer_event{};
    timer_event.data.fd = timer_manager.get_timer_fd();
    timer_event.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_manager.get_timer_fd(), &timer_event);
    epoll_event events[MAX_EVENTS];
    std::cout<<"Server run"<<std::endl;
    while(true){
        int n=epoll_wait(epoll_fd,events,MAX_EVENTS,-1);
        for(int i=0;i<n;i++){
            int fd=events[i].data.fd;
            if(fd==listen_fd){
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &client_len);
                if (client_fd < 0) {
                    perror("accept");
                    continue;           // 跳过当前错误，继续接受其他连接
                }
                set_nonblocking(client_fd);
                epoll_event event{};
                event.data.fd = client_fd;
                event.events = EPOLLIN;
                if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_fd,&event) < 0){
                    perror("epoll_ctl");
                    exit(1);
                }
            }else if(fd==timer_manager.get_timer_fd()){
                timer_manager.handle_expired();
            }else if(events[i].events & EPOLLIN){
                int client_fd=fd;
                thread_pool.enqueue([this,client_fd](){
                    handle_request(client_fd);
                });
            }
        }
    }

}


void Server::handle_request(int client_fd) {
    LOG_INFO("Handling request from client fd = " + std::to_string(client_fd));
    char buffer[BUFFER_SIZE] = {0};
    int bytes = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
        LOG_ERROR("Failed to read request from fd = " + std::to_string(client_fd));

        close(client_fd);
        return;
    }

    HttpRequest request;
    request.parse(buffer);

    HttpResponse response("www");
    std::string out_buf;
    response.make_response(request.get_path(), out_buf);

    write(client_fd, out_buf.c_str(), out_buf.size());

    if (response.file_data()) {
        write(client_fd, response.file_data(), response.file_len());
    }
    response.~HttpResponse();  // 手动析构以释放 mmap
    close(client_fd);
    timer_manager.add_timer(client_fd, timeout_ms, [=]() {
    LOG_WARN("Connection timeout, closing fd = " + std::to_string(client_fd));
    std::cout << "Closing idle connection: " << client_fd << std::endl;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
    });
}