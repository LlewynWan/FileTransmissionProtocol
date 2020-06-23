#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "protocol.h"
#include "manual.h"

bool is_blank(char c)
{
    return c == ' ' || c == '\n';
}

char** parse(char* command, int* argc)
{
    *argc = 0;
    int len = strlen(command);
    for (int i = 0; i < len; i++)
    {
        if (is_blank(command[i]))
            continue;
        
        if (i == len-1 || is_blank(command[i+1]))
            (*argc)++;
    }

    int cnt = 0;
    int prev = 0;
    char** argv = (char**)calloc(*argc, sizeof(char*));
    for (int i = 0; i < len; i++)
    {
        if (is_blank(command[i]))
            continue;

        if (i != 0 && is_blank(command[i-1]))
            prev = i;
        if (i != len-1 && is_blank(command[i+1]))
        {
            argv[cnt] = (char*)calloc(i-prev+2, sizeof(char));
            memset(argv[cnt], 0, sizeof(argv));
            strncpy(argv[cnt++], command+prev, i-prev+1);
        }
    }
    return argv;
}


/*Client Main Function*/
void start_client(char* address, int port)
{
    int clnt_sock;
    if ((clnt_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("socket creation failed");
        return;
    }
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(address);
    serv_addr.sin_port = htons(port);

    if (connect(clnt_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0)
    {
        perror("connection failed");
        return;
    }
    bool db_success;
    read(clnt_sock, &db_success, sizeof(bool));
    if (!db_success)
    {
        close(clnt_sock);
        return;
    }
    bool cwd_success;
    read(clnt_sock, &cwd_success, sizeof(bool));
    if (!cwd_success)
    {
        close(clnt_sock);
        return;
    }

    char cwd[INFO_LEN];
    memset(cwd, 0, sizeof(cwd));

    for (;;)
    {
        printf("%s >> ", cwd);

        char command[1024];
        fgets(command, 1024, stdin);
        
        int argc;
        char** argv = parse(command, &argc);

        if (argc == 0)
            continue;

        if (strcmp(argv[0], "exit") == 0)
        {
            if (argc != 1)
            {
                printf("invalid command\nusage: exit\n\n");
                continue;
            }
            write(clnt_sock, "EXIT", CMD_LEN);
            break;
        }

        if (strcmp(argv[0], "login") == 0)
        {
            if (argc != 3)
            {
                printf("invalid command\nusage: login [username] [password]\n\n");
                continue;
            }
            if (strlen(argv[1]) > CHAR_LEN)
            {
                printf("username should be no longer than 25 characters\n\n");
                continue;
            }
            write(clnt_sock, "LOGIN", CMD_LEN);
            write(clnt_sock, argv[1], CHAR_LEN);
            write(clnt_sock, argv[2], CHAR_LEN);

            char info[INFO_LEN];
            read(clnt_sock, info, INFO_LEN);
            if (strcmp(info, ERRMSG) == 0)
                printf("an error occured on server\n\n");
            else
                printf("%s\n\n", info);
            
            if (strcmp(info, "successfully loged in") == 0)
            {
                memset(cwd, 0, sizeof(cwd));
                write(clnt_sock, "CWD", CMD_LEN);
                read(clnt_sock, cwd, INFO_LEN);
            }
            continue;
        }

        if (strcmp(argv[0], "signup") == 0)
        {
            if (argc != 3)
            {
                printf("invalid command\nusage: signup [username] [password]\n\n");
                continue;
            }
            if (strlen(argv[1]) > CHAR_LEN || strlen(argv[2]) > CHAR_LEN)
            {
                printf("username or password should be no longer than 25 characters\n\n");
                continue;
            }
            write(clnt_sock, "SIGNUP", CMD_LEN);
            write(clnt_sock, argv[1], CHAR_LEN);
            write(clnt_sock, argv[2], CHAR_LEN);

            char info[INFO_LEN];
            read(clnt_sock, info, INFO_LEN);
            if (strcmp(info, ERRMSG) == 0)
                printf("an error occured on server\n\n");
            else
                printf("%s\n\n", info);

            continue;
        }

        if (strcmp(argv[0], "cwd") == 0)
        {
            if (argc != 1)
            {
                printf("invalid command\nusage: cwd\n\n");
                continue;
            }
            write(clnt_sock, "CWD", CMD_LEN);
            char cwd[INFO_LEN];
            read(clnt_sock, cwd, INFO_LEN);
            printf("%s\n\n", cwd);

            continue;
        }

        if (strcmp(argv[0], "cd") == 0)
        {
            if (argc != 2)
            {
                printf("invalid command\nusage: cd [path]\n\n");
                continue;
            }
            if (strlen(argv[1]) > INFO_LEN - 26)
            {
                printf("invalid path: too many characters\n\n");
                continue;
            }
            write(clnt_sock, "CHDIR", CMD_LEN);
            write(clnt_sock, argv[1], INFO_LEN);

            char info[INFO_LEN];
            memset(info, 0, sizeof(info));
            read(clnt_sock, info, INFO_LEN);
            printf("%s\n\n", info);
            
            memset(cwd, 0, sizeof(cwd));
            write(clnt_sock, "CWD", CMD_LEN);
            read(clnt_sock, cwd, INFO_LEN);

            continue;
        }

        if (strcmp(argv[0], "ls") == 0)
        {
            if (argc != 1 && argc != 2)
            {
                printf("invalid command\nusage: ls [directory]\n\n");
                continue;
            }
            write(clnt_sock, "LISTDIR", CMD_LEN);
            if (argc == 1)
                write(clnt_sock, " ", INFO_LEN);
            else
                write(clnt_sock, argv[1], INFO_LEN);

            char info[INFO_LEN*30];
            memset(info, 0, sizeof(info));
            read(clnt_sock, info, INFO_LEN*30);
            printf("%s\n\n", info);
            
            continue;
        }

        if (strcmp(argv[0], "delete") == 0)
        {
            if (argc != 2)
            {
                printf("invalid command\nusage: delete [path_to_file]\n\n");
                continue;
            }
            write(clnt_sock, "DELETE", CMD_LEN);
            write(clnt_sock, argv[1], INFO_LEN);
            char info[INFO_LEN];
            read(clnt_sock, info, INFO_LEN);
            printf("%s\n\n", info);

            continue;
        }

        if (strcmp(argv[0], "upload") == 0)
        {
            if (argc != 3)
            {
                printf("invalid command\nusage: upload [local_path_to_file] [remote_directory]\n\n");
                continue;
            }

            if (access(argv[1], R_OK) == -1)
            {
                printf("%s: file does not exist\n\n", argv[1]);
                continue;
            }

            write(clnt_sock, "UPLOAD", CMD_LEN);
            char* filename = parse_filename(argv[1]);
            write(clnt_sock, argv[2], INFO_LEN);
            write(clnt_sock, filename, INFO_LEN);
            char info[INFO_LEN];
            read(clnt_sock, info, INFO_LEN);
            if (strcmp(info, "TRANS") == 0)
            {
                printf("uploading...\n");
                send_file(clnt_sock, argv[1]);
                printf("upload complete\n\n");
            }
            else
                printf("%s\n\n", info);

            continue;
        }

        if (strcmp(argv[0], "download") == 0)
        {
            if (argc != 3)
            {
                printf("invalid command\nusage: download [local_directory] [remote_path_to_file]\n\n");
                continue;
            }

            struct stat sb;
            if (stat(argv[1], &sb) != 0 || !S_ISDIR(sb.st_mode))
            {
                printf("%s: directory does not exist\n\n", argv[1]);
                continue;
            }

            write(clnt_sock, "DOWNLOAD", CMD_LEN);
            write(clnt_sock, argv[2], INFO_LEN);
            
            char info[INFO_LEN];
            read(clnt_sock, info, INFO_LEN);
            if (strcmp(info, "TRANS") == 0)
            {
                char filename[INFO_LEN];
                memset(filename, 0, sizeof(filename));
                read(clnt_sock, filename, INFO_LEN);
                char filepath[INFO_LEN*2];
                memset(filepath, 0, sizeof(filepath));
                
                int argv1_len = strlen(argv[1]) - 1;
                while (argv1_len > 0 && argv[1][argv1_len] == '/')
                    argv1_len--;
                strncpy(filepath, argv[1], argv1_len+1);
                sprintf(filepath+strlen(filepath), "/%s", filename);
                printf("downloading...\n");
                recv_file(clnt_sock, filepath);
                printf("download complete\n\n");
            }
            else
                printf("%s\n\n", info);

            continue;
        }

        if (strcmp(argv[0], "help") == 0)
        {
            if (argc != 1)
            {
                printf("try: 'help' for manual\n\n");
                continue;
            }

            printf("%s\n\n", MANUAL);
            continue;
        }

        printf("command '%s' not found\ntry: 'help' for manual\n\n", argv[0]);
    }
    close(clnt_sock);
}


int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("usage: %s destination_ip destination_port\n", argv[0]);
        return 1;
    }
    struct sockaddr_in addr;
    if (inet_aton(argv[1], (struct in_addr*)&addr.sin_addr.s_addr) == 0)
    {
        perror("inet_aton");
        printf("%s isn't a valid IP address\n", argv[1]);
        return 1;
    }
    
    int len = strlen(argv[2]);
    for (int i = 0; i < len; i++)
        if (argv[2][i] < '0' || argv[2][i] > '9')
        {
            printf("%s isn't a valid port number\n", argv[2]);
            return 1;
        }
    int port = atoi(argv[2]);

    start_client(argv[1], port);
}
