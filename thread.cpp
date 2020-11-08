#include "thread.h"

bool ScanThread::open() {
    active_ = true;
    return true;
}

bool ScanThread::close() {
    active_ = false;
    return true;
}

void ScanThread::run() {

    if (!open()) return;

    fd_set readfds;
    int fd = client_sock;
    int state;
    struct    timeval tv;

    char data[BUF_SIZE] = {0};
    char buf[BUF_SIZE] = {0};
    bool isRecv = false;

    memset(buf, 0x00, BUF_SIZE);
    memcpy(buf, "1", 1);
    send_data(client_sock, buf);

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    while (active_)
    {
        FD_SET(fd, &readfds);
        //        tv.tv_sec = 1;
        //        tv.tv_usec = 0;

        if ((state = select(fd + 1, &readfds, 0, 0, &tv)) == -1)
        {
            perror("select() error");
            exit(0);
            break;
        }
        if (state == 0)
            continue;

        memset(data, 0x00, BUF_SIZE);
        isRecv = recv_data(client_sock, data);

        if(isRecv)
        {
            //qDebug() << "captured ok" << QString(data);
            emit captured(data);
        }
    }

    memset(buf, 0x00, BUF_SIZE);
    memcpy(buf, "5", 1);
    send_data(client_sock, buf);

    close();

}

