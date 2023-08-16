#ifndef SERVER_H
#define SERVER_H

#define SERVER_INTT_FAIL -1
#define PORT_REUSE 1

typedef struct FdInfo
{
    int fd;        // socket fd
    int epfd;      // epoll fd
}FdInfo;

// 初始化监听 fd
int init_listen_fd(unsigned short port);

// 运行epoll机制
int epoll_run(int lfd);

// 与客户端建立连接
void* accept_clinet(void *arg);

// 接收Http请求
void* rec_http_request(void *arg);

//释放FdInfo
void free_fd_info(FdInfo *fdInfo);

#endif
