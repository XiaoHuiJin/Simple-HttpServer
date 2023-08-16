#include <stdio.h>
#include <stdlib.h>
#include "server.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    int ret = 0;
    int lfd = 0;
    unsigned short port = 0;
    

    if (argc < 3)
    {
        printf("./a.out port path\n");
        return SERVER_INTT_FAIL;
    }

    // 1. 切换服务器的工作路径
    chdir(argv[2]);

    // 2.初始化监听fd
    port = atoi(argv[1]);
    lfd = init_listen_fd(port);
    if (lfd == SERVER_INTT_FAIL)
    {
        printf("server init failed!\n");
        return SERVER_INTT_FAIL;
    }

    // 2.启动服务器
    printf("server start ---------------------------\n");
    ret = epoll_run(lfd);
    if (ret == SERVER_INTT_FAIL)
    {
        printf("server start failed!\n");
        return SERVER_INTT_FAIL;
    }

    return 0;
}
