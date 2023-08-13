#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("./a.out port path\n");
        return -1;
    }

    // 1. 切换服务器的工作路径
    chdir(argv[2]);

    // 2.初始化监听 fd
    unsigned short port = atoi(argv[1]);
    int lfd = init_listen_fd(port);

    // 2.启动服务器
    printf("server start ---------------------------\n");
    int ret = epoll_run(lfd);

    return 0;
}
