#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QMap>
#include <QScrollBar>
#include <QFontDatabase>
#include <QMutex>
#include <QPair>
#include <QTimer>
#include "socket.h"
#include "thread.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct TalkerMap : QMap<QString, QString>{
    QMutex m;
};

typedef struct ap_map_info
{
    int channel;
    TalkerMap station_map;

}ap_info;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void graphInit();
    int client_sock;
    char data[BUF_SIZE];
    char buf[BUF_SIZE];
    QString selected_ap;
    bool isApScan;
    bool isStationScan;
    //QMap <QString, ap_map_info> ap_map;
    std::map<QString, ap_map_info> ap_map;
    ScanThread * scanThread_;


private slots:
    void processCaptured(char* data);
    void on_tableWidget_cellDoubleClicked(int row, int column);
    void RealtimeDataSlot();
    void graphClicked();
    void on_pushButton_clicked();

private:
    Ui::MainWindow *ui;
    QTimer DataTimer;
};


#endif // MAINWINDOW_H
