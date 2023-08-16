#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#define HTTP_RESPONSE_ERROR     -1

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