#include "server.h"
#include <arpa/inet.h>
#include "error.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <ctype.h>

// 初始化监听 fd
int init_listen_fd(unsigned short port)
{
    // 创建监听的fd
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == lfd)
    {
        error_print("socket error!");
        return -1;
    }

    // 设置端口复用
    int opt = 1;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (-1 == ret)
    {
        error_print("setsockopt error!");
        return -1;
    }

    // 绑定lfd
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd, (struct sockadd *)&addr, sizeof(addr));
    if (-1 == ret)
    {
        error_print("bind error!");
        return -1;
    }

    // 设置监听
    ret = listen(lfd, 128);
    if (-1 == ret)
    {
        error_print("listen error!");
        return -1;
    }

    return lfd;
}

// 运行epoll机制
int epoll_run(int lfd)
{
    // 1.创建epoll实例
    int ep_fd = epoll_create(1);
    if (-1 == ep_fd)
    {
        error_print("epoll_create error!");
        return -1;
    }

    // 2.lfd上树
    struct epoll_event ev;
    ev.data.fd = lfd;
    ev.events = EPOLLIN;
    int ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, lfd, &ev);
    if (-1 == ret)
    {
        error_print("epoll add listen fd error!");
        return -1;
    }

    struct epoll_event evs[1024];
    int size = sizeof(evs) / sizeof(struct epoll_event);
    // 3.检测
    while (1)
    {
        int num = epoll_wait(ep_fd, evs, size, -1); // 没有读事件时阻塞
        if (-1 == num)
        {
            error_print("epoll_wait error!");
            return -1;
        }

        for (int i = 0; i < num; i++)
        {
            int fd = evs[i].data.fd;
            if (fd == lfd)
            {
                // 建立新链接
                accept_clinet(lfd, ep_fd);
            }
            else
            {
                // 接收对端的数据
                rec_http_request(fd, ep_fd);
            }
        }
    }

    return 0;
}

// 与客户端建立连接
int accept_clinet(int lfd, int ep_fd)
{
    // 1. 得到cfd
    int cfd = accept(lfd, NULL, NULL);
    if (-1 == cfd)
    {
        error_print("accept error!");
        return -1;
    }

    // 2. 设置非阻塞
    int flag = fcntl(cfd, F_GETFL, 0);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    // 3.将cfd添加到epoll
    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET; // 边缘非阻塞
    int ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, cfd, &ev);
    if (-1 == ret)
    {
        error_print("epoll add connect fd error!");
        return -1;
    }

    return 0;
}

// 接收Http请求
int rec_http_request(int cfd, int ep_fd)
{
    printf("rev request from client---------------------\n");
    char tmp[1024] = {0};  // 存放接收的数据
    char buff[4096] = {0}; // 写入缓冲区的数据
    int len = 0;
    int total = 0;

    while ((len = recv(cfd, tmp, sizeof(tmp), 0)) > 0)
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
        parse_line(buff, cfd);
    }
    else if (len == 0)
    {
        // 客户端断开了连接
        epoll_ctl(ep_fd, EPOLL_CTL_DEL, cfd, NULL);
        close(cfd);
        return -1;
    }
    else
    {
        error_print("recv error!");
        return -1;
    }

    return 0;
}

// 解析请求行
int parse_line(const char *line, int cfd)
{
    char method[12] = {0};
    char path[1024] = {0};

    printf("parse_line: buff is: %s\n", line);
    sscanf(line, "%[^ ] %[^ ]", method, path);
    printf("method: %s, path: %s\n", method, path);

    //对中文进行解码
    char decode_path[1024] = { 0 };
    decode_msg(path, decode_path);
    if (strcasecmp(method, "get") == 0)
    {
        // get请求
        process_get(decode_path, cfd);
    }
    else if (strcasecmp(method, "post") == 0)
    {
        // post请求
    }
    else
    {
        error_print("method error!");
        return -1;
    }

    return 0;
}

// 处理get请求
int process_get(const char *path, int cfd)
{
    printf("path: %s\n", path);
    // 处理客户端请求的静态资源
    char *file = NULL;
    if (strcmp(path, "/") == 0)
    {
        file = "./";
    }
    else
    {
        file = path + 1;
    }

    // 获取文件的属性
    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1)
    {
        // 文件不存在 --返回404页面
        send_head_msg(cfd, 404, "Not Found", "text/html; charset=utf-8", -1);
        send_file("404.html", cfd);
        return 0;
    }

    // 判断文件类型
    if (S_ISDIR(st.st_mode))
    {
        // 目录，将目录内容发送给客户端
        // printf("%s 是目录\n", file);
        send_head_msg(cfd, 200, "OK", get_file_type(".html"), st.st_size);
        send_dir(file, cfd);
    }
    else
    {
        // 文件，将文件内容发送给客户端
        send_head_msg(cfd, 200, "OK", get_file_type(file), st.st_size);
        send_file(file, cfd);
    }

    return 0;
}

// 发送文件内容到客户端
int send_file(const char *file, int cfd)
{
    // 打开文件
    int fd = open(file, O_RDONLY);
    if (fd == -1)
    {
        error_print("file open error!");
        return -1;
    }

    off_t offset = 0;
    off_t size = lseek(fd, 0, SEEK_END);
    printf("size is :%ld\n", size);
    lseek(fd, 0, SEEK_SET);
    while (offset < size)
    {
        int ret = sendfile(cfd, fd, &offset, size-offset);
        if (ret == -1 && errno == EAGAIN)
        {
            printf("file send finish!\n");
        }
    }
    
    close(fd);

    return 0;
}

// 发送响应头
int send_head_msg(int cfd, int status, const char *desc, const char *type, int length)
{
    // printf("执行send_head_msg-------------\n");
    char buff[4096] = {0};
    sprintf(buff, "http/1.1 %d %s\r\n", status, desc); // 添加响应行
    sprintf(buff + strlen(buff), "content-type: %s\r\n", type);
    sprintf(buff + strlen(buff), "content-length: %d\r\n", length);
    sprintf(buff + strlen(buff), "\r\n"); // 空行

    // printf("响应头：%s\n", buff);

    send(cfd, buff, strlen(buff), 0);

    return 0;
}

/*
 <html>
    <head>
        <title> </title>
    </head>
    <body>
        <table>
            <tr>
                <td></td>
            </tr>
        </table>
    </body>
 </html>

*/

// 发送目录
int send_dir(const char *dir, int cfd)
{
    char buff[4096] = {0};
    sprintf(buff, "<html><head><title>WebServer</title></head><body><table>");
    struct dirent **nameList;
    int num = scandir(dir, &nameList, NULL, alphasort);
    if (num == -1)
    {
        error_print("scandir error!");
        return -1;
    }

    // printf("num = %d\n", num);

    for (int i = 0; i < num; i++)
    {
        char *name = nameList[i]->d_name;
        // printf("filename: %s\n", name);
        // 判断是否是目录
        struct stat st;
        stat(name, &st);
        if (S_ISDIR(st.st_mode))
        {
            // 目录
            //printf("目录: %s\n");
            sprintf(buff + strlen(buff), "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        }
        else
        {
            // 文件
            //printf("文件: \n");
            sprintf(buff + strlen(buff), "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        }
        //printf("buff: %s\n", buff);
        send(cfd, buff, strlen(buff), 0);
        memset(buff, 0, sizeof(buff));
        free(nameList[i]);
    }
    sprintf(buff, "</table></body></html>");
    send(cfd, buff, strlen(buff), 0);
    // printf("buff: %s\n", buff);

    free(nameList);
    return 0;
}

//获取文件的类型
const char *get_file_type(const char *filename)
{
    char *type = strrchr(filename, '.');
    if (type == NULL)
        return "text/plain; charset=utf-8"; //纯文本
    if (strcmp(type, ".html") == 0 || strcmp(type, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(type, ".jpeg") == 0 || strcmp(type, ".jpg") == 0)
        return "image/jpeg";
    if (strcmp(type, ".gif") == 0)
        return "image/gif";
    if (strcmp(type, ".mp4") == 0)
        return "video/mpeg4";
    if (strcmp(type, ".pdf") == 0)
        return "application/pdf";
    else 
        return "text/plain; charset=utf-8"; //纯文本
}

int hex_to_int(char c)
{
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('A' <= c && c <= 'F')
        return c - 'A' + 10;
    if ('a' <= c && c <= 'f')
        return c - 'a' + 10;  

    return 0;  
}

//对中文进行解码
void decode_msg(char *src, char *dec)
{

    for (; *src != '\0'; dec ++, src ++) 
    {
        if (src[0] == '%' && isxdigit(src[1]) && isxdigit(src[2]))
        {
            // /%E9%A3%8E%E6%99%AF.jpg
            //将16进制转换成10进制
            *dec = hex_to_int(src[1]) * 16 + hex_to_int(src[2]);
            src += 2;
        }
        else 
        {
            *dec = *src;
        }
        
    }
    printf("dec = %s\n", dec);
    return;
}