#include "server.h"
#include <arpa/inet.h>
#include "error_print.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>

// 初始化监听 fd
int init_listen_fd(unsigned short port)
{
    int ret = 0;
    int lfd = 0;

    // 创建监听的fd
    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        error_print("socket listen fd error!");
        return SERVER_INTT_FAIL;
    }

    // 设置端口复用s
    int opt = PORT_REUSE;
    ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret == -1)
    {
        error_print("setsockopt error!");
        return SERVER_INTT_FAIL;
    }

    // 绑定lfd
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd, (struct sockadd *)&addr, sizeof(addr));
    if (ret == -1)
    {
        error_print("bind error!");
        return SERVER_INTT_FAIL;
    }

    // 设置监听
    ret = listen(lfd, 128);
    if (ret == -1)
    {
        error_print("listen error!");
        return SERVER_INTT_FAIL;
    }

    return lfd;
}

// 运行epoll机制
int epoll_run(int lfd)
{
    int ret = 0;

    // 1.创建epoll实例
    int epfd = epoll_create(1);
    if (epfd == -1)
    {
        error_print("epoll_create error!");
        return SERVER_INTT_FAIL;
    }

    // 2.lfd上树
    struct epoll_event ev;
    ev.data.fd = lfd;
    ev.events = EPOLLIN;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if (ret == -1)
    {
        error_print("epoll add listen fd error!");
        return SERVER_INTT_FAIL;
    }

    struct epoll_event evs[1024];
    int size = sizeof(evs) / sizeof(struct epoll_event);

    // 3.检测
    while (1)
    {
        int num = epoll_wait(epfd, evs, size, -1); // 没有读事件时阻塞
        if (num == -1)
        {
            error_print("epoll_wait error!");
            return SERVER_INTT_FAIL;
        }

        for (int i = 0; i < num; i++)
        {
            pthread_t pid = 0;
            FdInfo *fdInfo = (FdInfo *) malloc(sizeof(FdInfo));
            if (fdInfo == NULL)
            {
                error_print("fd info malloc error!");
                return SERVER_INTT_FAIL;
            }

            fdInfo->fd = evs[i].data.fd;
            fdInfo->epfd = epfd;
            if (fdInfo->fd == lfd)
            {
                // 建立新链接
                pthread_create(&pid, NULL, accept_clinet, fdInfo);
            }
            else
            {
                // 接收对端的数据
                pthread_create(&pid, NULL, rec_http_request, fdInfo);
            }
        }
    }

    return 0;
}

// 与客户端建立连接
void* accept_clinet(void *arg)
{
    int cfd = 0;
    int ret = 0;

    FdInfo *fdInfo = (FdInfo *)(arg);
    // 1. 得到cfd
    cfd = accept(fdInfo->fd, NULL, NULL);
    if (cfd == -1)
    {
        error_print("accept error!");
        free_fd_info(fdInfo);
        return;
    }

    // 2. 设置非阻塞
    int flag = fcntl(cfd, F_GETFL, 0);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    // 3.将cfd添加到epoll
    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET; // 边缘非阻塞
    ret = epoll_ctl(fdInfo->epfd, EPOLL_CTL_ADD, cfd, &ev);
    if (ret == -1)
    {
        error_print("epoll add connect fd error!");
        free_fd_info(fdInfo);
        return;
    }
    free_fd_info(fdInfo);;

}

// 接收Http请求
void* rec_http_request(void *arg)
{
    printf("rev request from client---------------------\n");


    FdInfo *fdInfo = (FdInfo *)(arg);
    char tmp[1024] = {0};  // 存放接收的数据
    char buff[4096] = {0}; // 写入缓冲区的数据
    int len = 0;
    int total = 0;

    while ((len = recv(fdInfo->fd, tmp, sizeof(tmp), 0)) > 0)
    {
        if ((total + len) < sizeof(buff))
        {
            memcpy(buff + total, tmp, len);
        }
        total += len;
    }

    // printf("data is: %s\n", buff);
    // 判断数据是否接收完毕
    if (len == -1 && errno == EAGAIN)
    {
        // 解析请求行
        char *pos = strstr(buff, "\r\n");
        int len = pos - buff;
        buff[len] = '\0';
        parse_line(buff, fdInfo->fd);
    }
    else if (len == 0)
    {
        // 客户端断开了连接
        epoll_ctl(fdInfo->epfd, EPOLL_CTL_DEL, fdInfo->fd, NULL);
        close(fdInfo->fd);
        free_fd_info(fdInfo);
        return;
    }
    else
    {
        error_print("recv error!");
        return;
    }
    free_fd_info(fdInfo);
}

//释放FdInfo
void free_fd_info(FdInfo *fdInfo)
{
    if (fdInfo != NULL) 
    {
        free(fdInfo);
        fdInfo = NULL;
    }
}