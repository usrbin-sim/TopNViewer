#pragma once

#include <QThread>
#include <QDebug>
#include <socket.h>
#include "socket.h"

struct ScanThread : QThread {
    Q_OBJECT

public:
    ScanThread(int sock) {
        client_sock = sock;
    }

    ~ScanThread() override {
    }

    bool active_{false};
    int client_sock;
protected:
    bool open();
    bool close();

protected:
    void run() override;

public:
signals:
    void captured(char * data);
};


