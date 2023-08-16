#include "http_response.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

// 解析请求行
int parse_line(const char *line, int cfd)
{
    int ret = 0;

    char method[12] = {0};
    char path[1024] = {0};

    // printf("parse_line: buff is: %s\n", line);
    sscanf(line, "%[^ ] %[^ ]", method, path);
    // printf("method: %s, path: %s\n", method, path);

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
        return HTTP_RESPONSE_ERROR;
    }

    return ret;
}

// 处理get请求
int process_get(const char *path, int cfd)
{
    int ret = 0;

    // printf("path: %s\n", path);
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
    ret = stat(file, &st);
    if (ret == -1)
    {
        // 文件不存在 --返回404页面
        send_head_msg(cfd, 404, "Not Found", "text/html; charset=utf-8", -1);
        send_file("404.html", cfd);
        return -1;
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

    return ret;
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
            sprintf(buff + strlen(buff), "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        }
        else
        {
            // 文件
            sprintf(buff + strlen(buff), "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        }
        send(cfd, buff, strlen(buff), 0);
        memset(buff, 0, sizeof(buff));
        free(nameList[i]);
    }
    sprintf(buff, "</table></body></html>");
    send(cfd, buff, strlen(buff), 0);

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
}