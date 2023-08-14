#ifndef _SERVER_H
#define _SERVER_H

#include <pthread.h>

typedef struct FD_INFO
{
    int fd;             // socket fd
    int ep_fd;          // epoll fd
    pthread_t pid;      // 线程id
} FD_INFO;


//初始化监听 fd
int init_listen_fd(unsigned short int pot);

//运行epoll机制
int epoll_run(int lfd);

//与客户端建立连接
void* accept_clinet(void *arg);

//接收Http请求协议
void* rec_http_request(void *arg);

//解析Http请求行
int parse_line(const char *line, int cfd);

//处理get请求
int process_get(const char *path, int cfd);

//发送文件内容到客户端
int send_file(const char *file, int cfd);

//发送响应头
int send_head_msg(int cfd, int status, const char *desc, const char *type, int length);

//发送目录
int send_dir(const char *dir, int cfd);

//获取文件的类型
const char *get_file_type(const char *filename);

//对中文进行解码
void decode_msg(char *src, char *dec);

#endif