#include "socket.h"

bool connect_sock(int * client_sock, int server_port)
{
    bool ret = true;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);
    if ((*client_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) // make client socket
        exit(1);

    memset(&client_addr, 0x00, addr_size);

    memset(&server_addr, 0x00, sizeof(server_addr));

    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (connect(*client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) // connect server socket
    {
        ret = false;
    }

    getsockname(*client_sock, (struct sockaddr *)&client_addr, &addr_size); // get client port

    return ret;
}

bool send_data(int client_sock, char *data)
{
    char result[BUF_SIZE] = {0};
    size_t data_length = strlen(data);
    result[0] = (data_length >> 8) & 0xFF;
    result[1] = data_length & 0xFF;
    memcpy(result + 2, data, strlen(data));

    if (write(client_sock, result, strlen(data) + 2) <= 0)
    {
        return false;
    }
    return true;
}

bool recv_data(int client_sock, char *data)
{
    unsigned char buf[BUF_SIZE] = {0};
    char buf_data[BUF_SIZE] = {0};
    char result[BUF_SIZE] = {0};
    char result_data[BUF_SIZE] = {0};
    size_t data_length = 0;
    size_t temp_length = 2;
    int read_size;


    temp_length = 2;
    while(temp_length > 0)
    {
        memset(buf, 0x00, BUF_SIZE);
        if ((read_size = read(client_sock, buf, temp_length)) <= 0)
        {
            return false;
        }

        memcpy(buf_data+(2-temp_length), buf, read_size);

        temp_length -= read_size;

    }

    data_length = (buf_data[0] << 8) + buf_data[1];

    temp_length = data_length;
    while (temp_length > 0)
    {
        memset(result, 0x00, BUF_SIZE);
        if ((read_size = read(client_sock, result, temp_length)) <= 0)
        {
            return false;
        }

        temp_length -= read_size;

        strcat(result_data, result);
    }

    result_data[data_length] = '\0';

    memcpy(data, result_data, BUF_SIZE);

    return true;
}
