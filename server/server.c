#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>

#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <mysql/mysql.h>

#include "protocol.h"

#define MAX_THREAD_CNT 1000

#define MYSQL_USER "root"
#define MYSQL_SERVER "localhost"
#define MYSQL_PASSWORD "password"
#define MYSQL_DATABASE "FTP"

#define QUERY_LEN 1024


int mysql_select(MYSQL* conn, char* query, MYSQL_RES** res)
{
    if (mysql_query(conn, query))
    { 
        perror(mysql_error(conn));
        return -1;
    }

    *res = mysql_store_result(conn);
    return 0;
}


int mysql_insert(MYSQL* conn, char* query)
{
    if (mysql_query(conn, query))
    {
        perror(mysql_error(conn));
        return -1;
    }
    return 0;
}


bool login(MYSQL* conn, int clnt_sock)
{
    char username[CHAR_LEN+1];
    memset(username, 0, sizeof(username));
    read(clnt_sock, username, CHAR_LEN);
    char password[CHAR_LEN+1];
    memset(password, 0, sizeof(password));
    read(clnt_sock, password, CHAR_LEN);

    MYSQL_RES* res;
    char query[QUERY_LEN];
    memset(query, 0, sizeof(query));
    sprintf(query, "SELECT * FROM users WHERE username=\"%s\";", username);

    if (mysql_select(conn, query, &res) < 0)
    { 
        write(clnt_sock, ERRMSG, INFO_LEN);
        return false;
    }

    if (res == NULL || mysql_num_rows(res) == 0)
    {
        write(clnt_sock, "account not found", INFO_LEN);
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (strcmp(row[1], password) == 0)
    {
        write(clnt_sock, "successfully loged in", INFO_LEN);
        return true;
    }
    else
    {
        write(clnt_sock, "incorrect username or password", INFO_LEN);
        return false;
    }
}

void signup(MYSQL* conn, int clnt_sock)
{
    char username[CHAR_LEN+1];
    memset(username, 0, sizeof(username));
    read(clnt_sock, username, CHAR_LEN);
    char password[CHAR_LEN+1];
    memset(password, 0, sizeof(password));
    read(clnt_sock, password, CHAR_LEN);

    MYSQL_RES* res;
    char query[QUERY_LEN];
    memset(query, 0, sizeof(query));
    sprintf(query, "SELECT * FROM users WHERE username=\"%s\";", username);

    if (mysql_select(conn, query, &res) < 0)
    {
        write(clnt_sock, ERRMSG, INFO_LEN);
        return;
    }

    if (res != NULL && mysql_num_rows(res) != 0)
    {
        write(clnt_sock, "account already exits", INFO_LEN);
        return;
    }

    memset(query, 0, sizeof(query));
    sprintf(query, "INSERT INTO users VALUE (\"%s\", \"%s\");", username, password);
    if (mysql_insert(conn, query) < 0)
        write(clnt_sock, ERRMSG, INFO_LEN);
    else
        write(clnt_sock, "successfully signed up", INFO_LEN);
}


char* change_directory(char* cwd, char* path)
{
    char* new_cwd = (char*)calloc(INFO_LEN, sizeof(char));
    memset(new_cwd, 0, sizeof(new_cwd));
    
    if (strlen(path) == 0)
    {
        strcpy(new_cwd, cwd);
        return new_cwd;
    }
    if (path[0] == '/')
    {
        int tail = strlen(path) - 1;
        while (tail > 0 && path[tail] == '/')
            tail --;
        strncpy(new_cwd, path, tail+1);
        return new_cwd;
    }

    int head = 0;
    int plen = strlen(path);
    int tail = strlen(cwd)-1;
    for (;;)
    {
        if (plen - head < 2)
            break;
        if (path[head] != '.' || path[head+1] != '.')
            break;
        
        head += 2;
        if (plen - head > 0 && path[head] == '/')
            head ++;

        while (tail > 0 && cwd[tail] != '/')
            tail --;
        if (tail > 0)
            tail --;
    }

    strncat(new_cwd, cwd, tail+1);
    while (plen > 0 && path[plen-1]=='/')
        plen --;

    if (plen - head >= 2 && path[head] == '.' && path[head+1] == '/')
    {
        if (tail > 0 && plen - head > 2)
            strcat(new_cwd, "/");
        strncat(new_cwd, path+head+2, plen-head-2);
    }
    else
    {
        if (tail > 0 && plen - head > 0)
            strcat(new_cwd, "/");
        strncat(new_cwd, path+head, plen-head);
    }

    return new_cwd;
}


void *clnt_handler(void* args)
/*
 * Main Client Handler
 */
{
    int clnt_sock = *(int*)args;

    MYSQL* conn = mysql_init(NULL);
    bool is_connected = mysql_real_connect(conn, MYSQL_SERVER, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DATABASE, 0, NULL, 0);
    
    write(clnt_sock, &is_connected, sizeof(bool));
    if (!is_connected)
    {
        perror("mysql connect failed");
        close(clnt_sock);
        return NULL;
    }

    bool is_logedin = false;
    char cwd[INFO_LEN];
    memset(cwd, 0, sizeof(cwd));
    bool cwd_success = (getcwd(cwd, sizeof(cwd)) != NULL);
    write(clnt_sock, &cwd_success, sizeof(bool));
    if (!cwd_success)
    {
        perror("getcwd() error");
        close(clnt_sock);
        return NULL;
    }

    for (;;)
    {
        char command[CMD_LEN];
        if (read(clnt_sock, command, CMD_LEN) <= 0)
            break;
        
        if (strcmp(command, "EXIT") == 0)
            break;
        
        if (strcmp(command, "LOGIN") == 0)
            is_logedin |= login(conn, clnt_sock);
        
        if (strcmp(command, "SIGNUP") == 0)
            signup(conn, clnt_sock);

        if (strcmp(command, "DOWNLOAD") == 0)
        {
            char filepath[INFO_LEN];
            read(clnt_sock, filepath, INFO_LEN);
            if (!is_logedin)
            {
                write(clnt_sock, "please log in first", INFO_LEN);
                continue;
            }

            if (access(filepath, R_OK) == -1)
            {
                write(clnt_sock, "file does not exist", INFO_LEN);
                continue;
            }

            char* new_cwd = change_directory(cwd, filepath);
            char* filename = parse_filename(new_cwd);

            write(clnt_sock, "TRANS", INFO_LEN);
            write(clnt_sock, filename, INFO_LEN);
            send_file(clnt_sock, filepath);
        }

        if (strcmp(command, "UPLOAD") == 0)
        {
            char filepath[INFO_LEN*2];
            read(clnt_sock, filepath, INFO_LEN);
            char filename[INFO_LEN];
            read(clnt_sock, filename, INFO_LEN);
            if (!is_logedin)
            {
                write(clnt_sock, "please log in first", INFO_LEN);
                continue;
            }

            char* new_cwd = change_directory(cwd, filepath);
            
            struct stat sb;
            if (stat(new_cwd, &sb) != 0 || !S_ISDIR(sb.st_mode))
            {
                char info[INFO_LEN*2+26];
                sprintf(info, "%s: directory does not exist", filepath);
                write(clnt_sock, info, INFO_LEN);
                continue;
            }

            write(clnt_sock, "TRANS", INFO_LEN);
            sprintf(filepath+strlen(filepath), "/%s", filename);
            recv_file(clnt_sock, filepath);
        }

        if (strcmp(command, "CWD") == 0)
        {
            if (!is_logedin)
            {
                write(clnt_sock, "please log in first", INFO_LEN);
                continue;
            }
            write(clnt_sock, cwd, INFO_LEN);
        }

        if (strcmp(command, "CHDIR") == 0)
        {
            char dir_path[INFO_LEN];
            read(clnt_sock, dir_path, INFO_LEN);
            if (!is_logedin)
            {
                write(clnt_sock, "please log in first", INFO_LEN);
                continue;
            }
            char* new_cwd = change_directory(cwd,dir_path);

            struct stat sb;
            if (stat(new_cwd, &sb) != 0 || !S_ISDIR(sb.st_mode))
            {
                char info[INFO_LEN+26];
                sprintf(info, "%s: directory does not exist", dir_path);
                write(clnt_sock, info, INFO_LEN);
                continue;
            }
            memset(cwd, 0, sizeof(cwd));
            strcpy(cwd, new_cwd);
            write(clnt_sock, "current working directory changed", INFO_LEN);
        }

        if (strcmp(command, "LISTDIR") == 0)
        {
            char dir_path[INFO_LEN];
            read(clnt_sock, dir_path, INFO_LEN);
            if (!is_logedin)
            {
                write(clnt_sock, "please log in first", INFO_LEN);
                continue;
            }

            char *new_cwd = cwd;
            if (strcmp(dir_path, " ") != 0)
                new_cwd = change_directory(cwd, dir_path);
            
            DIR* d;
            struct dirent *dir;
            char info[INFO_LEN*30];
            memset(info, 0, sizeof(info));
            d = opendir(new_cwd);
            if (d) {
                while ((dir = readdir(d)) != NULL)
                {
                    if (dir->d_name[0] == '.')
                        continue;
                    sprintf(info+strlen(info), "%s\n", dir->d_name);
                }
                closedir(d);
                write(clnt_sock, info, INFO_LEN*30);
            }
            else {
                sprintf(info, "%s: directory does not exist\n", dir_path);
                write(clnt_sock, info, INFO_LEN*30);
                continue;
            }
        }

        if (strcmp(command, "DELETE") == 0)
        {
            char filepath[INFO_LEN];
            read(clnt_sock, filepath, INFO_LEN);
            if (!is_logedin)
            {
                write(clnt_sock, "please log in first", INFO_LEN);
                continue;
            }
            
            char *new_cwd = change_directory(cwd, filepath);
            if (access(new_cwd, F_OK) == -1)
            {
                write(clnt_sock, "file does not exist", INFO_LEN);
                continue;
            }
            char cmd[INFO_LEN];
            memset(cmd, 0, sizeof(cmd));
            sprintf(cmd, "rm -rf %s", new_cwd);
            system(cmd);

            write(clnt_sock, "file removed", INFO_LEN);
        }
    }
    close(clnt_sock);
}


/*Main Server Function*/
void start_server(unsigned long int address, int port)
{
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(address);
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 5000);

    int top = 0;
    struct thread
    {
        bool is_alive;
        pthread_t tid;
    };
    struct thread thread_queue[MAX_THREAD_CNT];
    memset(thread_queue, 0, sizeof(thread_queue));

    for (;;)
    {
        struct sockaddr_in clnt_addr;
        socklen_t clnt_addr_size = sizeof(clnt_addr);
        
        int clnt_sock = accept(sockfd, (struct sockaddr*)&clnt_addr, &clnt_addr_size);

        if (thread_queue[top].is_alive = true);
        {
            pthread_join(thread_queue[top].tid, NULL);
            thread_queue[top].is_alive = false;
        }
        thread_queue[top].is_alive = true;
        pthread_create(&thread_queue[top++].tid, NULL, clnt_handler, (void*)&clnt_sock);
        top %= MAX_THREAD_CNT;
    }

    close(sockfd);
}


int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("usage: %s port\n", argv[0]);
        return 1;
    }

    int len = strlen(argv[1]);
    for (int i = 0; i < len; i++)
        if (argv[1][i] < '0' || argv[1][i] > '9')
        {
            printf("%s isn't a valid port number\n", argv[1]);
            return 1;
        }
    int port = atoi(argv[1]);

    start_server(INADDR_ANY, port);
    return 0;
}
