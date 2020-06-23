#include <sys/stat.h>

#define CMD_LEN 10
#define CHAR_LEN 25
#define INFO_LEN 1024
#define MAX_BUFF_SIZE 32768
#define BUFF_UNIT_SIZE 1024
#define MAX_DATA_SIZE (MAX_BUFF_SIZE - sizeof(struct tcp_header))

#define ERRMSG "ERROR"

struct tcp_header
{
    int num;
    int size;
};

struct tcp_packet
{
    struct tcp_header header;
    char data[MAX_DATA_SIZE];
};

int compare(const void *packet1, const void *packet2)
{
    return (*(struct tcp_packet*)packet1).header.num - (*(struct tcp_packet*)packet2).header.num;
}


char* parse_filename(char* path)
{
    char* filename = (char*)calloc(INFO_LEN, sizeof(char));
    memset(filename, 0, sizeof(filename));

    int head = strlen(path) - 1;
    while (head > 0 && path[head] != '/')
        head --;
    if (head == 0 && path[head] != '/')
        strcpy(filename, path);
    else
        strcpy(filename, path+head+1);

    return filename;
}


void send_file(int clnt_sock, char* filepath)
{
    struct stat st;
    stat(filepath, &st);
    int file_size = st.st_size;

    FILE* fp = fopen(filepath, "rb");
    int num_packets = file_size / MAX_BUFF_SIZE;
    if (file_size % MAX_BUFF_SIZE)
        num_packets ++;

    write(clnt_sock, &num_packets, sizeof(int));

    for (int i = 0; i < num_packets; i++)
    {
        char buffer[MAX_BUFF_SIZE];

        int packet_size;
        int offset = MAX_BUFF_SIZE * i;
        if (offset + MAX_BUFF_SIZE > file_size)
            packet_size = file_size % MAX_BUFF_SIZE;
        else
            packet_size = MAX_BUFF_SIZE;
        fseek(fp, offset, SEEK_SET);

        fread(buffer, sizeof(char), packet_size, fp);
        write(clnt_sock, &packet_size, sizeof(int));
        write(clnt_sock, buffer, packet_size);

        if ((i * MAX_BUFF_SIZE % (BUFF_UNIT_SIZE * 64)) == 0)
            read(clnt_sock, buffer, 1);
    }
}


void recv_file(int clnt_sock, char* filepath)
{
    int num_packets;
    read(clnt_sock, &num_packets, sizeof(int));

    FILE* fp = fopen(filepath, "wb");
    for (int i = 0; i < num_packets; i++)
    {
        int packet_size;
        read(clnt_sock, &packet_size, sizeof(int));
        char* buffer = (char*)calloc(packet_size, sizeof(char));
        read(clnt_sock, buffer, packet_size);
        fwrite(buffer, sizeof(char), packet_size, fp);
        if ((i * MAX_BUFF_SIZE % (BUFF_UNIT_SIZE * 64)) == 0)
            write(clnt_sock, (char*)malloc(sizeof(char)), 1);
    }
    fclose(fp);
}

