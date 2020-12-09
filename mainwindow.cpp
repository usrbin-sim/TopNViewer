#include "mainwindow.h"
#include "ui_mainwindow.h"

class MyScrollBar : public QScrollBar
{
public:
    MyScrollBar(QWidget * parent): QScrollBar(parent) {}

protected:
    QSize sizeHint() const override { return QSize(64, 0); }
    QSize minimumSizeHint() const override { return QSize(64, 0); }
};

void stringToHex(QString str, uint8_t * dst)
{
    QByteArray byteName = str.toLocal8Bit();
    char * src = byteName.data();

    for(int i = 0; i < 6; i++){
        dst[i] = src[i*3] << 1 | src[i*3+1];
    }
}
QColor makeColor(QString str){

    uint8_t mac[6] = {0,};

    stringToHex(str, mac);

    int r = (mac[0] + mac[1]) / 2;
    int g = (mac[2] + mac[3]) / 2;
    int b = (mac[4] + mac[4]) / 2;

    return QColor(r, g, b);
}

#ifdef Q_OS_ANDROID
bool copyFileFromAssets(QString fileName, QFile::Permissions permissions) {
    QString sourceFileName = QString("assets:/") + fileName;
    QFile sFile(sourceFileName);
    QFile dFile(fileName);
    if (!dFile.exists()) {
        if (!sFile.exists()) {
            QString msg = QString("src file(%1) not exists").arg(sourceFileName);
            qDebug() << qPrintable(msg);
            return false;
        }

        sFile.copy(fileName);
        QFile::setPermissions(fileName, permissions);
    }
    return true;
}
#endif // Q_OS_ANDROID


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

#ifdef Q_OS_ANDROID
    copyFileFromAssets("topnviewerd.sh", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    copyFileFromAssets("topnviewerd", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    copyFileFromAssets("iwlist", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    copyFileFromAssets("iwconfig", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
#endif // Q_OS_ANDROID

    QHeaderView *verticalHeader = ui->tableWidget->verticalHeader();

#ifdef Q_OS_ANDROID
    verticalHeader->setDefaultSectionSize(80);
    verticalHeader->setVisible(false);
#endif

    ui->tableWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn); // Always show scroll bar
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers); // Disable editing

    QHeaderView* hw = ui->tableWidget->horizontalHeader();
    hw->setSectionResizeMode(0, QHeaderView::Stretch); // Set SSID size policy
    hw->setSectionResizeMode(1, QHeaderView::Fixed); // Set MAC size policy
    hw->setSectionResizeMode(2, QHeaderView::Fixed); // Set SELECT size policy

    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont); // Use system fixed width font
    ui->tableWidget->setFont(fixedFont);

#ifdef Q_OS_ANDROID
    ui->tableWidget->setVerticalScrollBar(new MyScrollBar(ui->tableWidget->verticalScrollBar())); // Big scroll bar
#endif // Q_OS_ANDROID


    // start Server
    {
        system("su -c \"/data/data/org.lucy.topnviewer/files/topnviewerd.sh&\"");
        usleep(500000);
    }

    char tmp_buf[1024] = {0,};
    int ret = 0;

    memset(buf, 0x00, BUF_SIZE);
    recv_data(client_sock, buf);
    if (!memcmp(buf, "5", 1)){
        QMessageBox MsgBox;
        MsgBox.setWindowTitle("Error");
        MsgBox.setText("pcap open failed.");
        MsgBox.setStandardButtons(QMessageBox::Ok);
        MsgBox.setDefaultButton(QMessageBox::Ok);
        if ( MsgBox.exec() == QMessageBox::Ok ){
            exit(1);
        }
    }

    ret = 0;
    FILE *fp = popen("su -c \"ps | grep topnviewerd\"", "rt");
    while(fgets(tmp_buf, 1024,fp)){
        if(strstr(tmp_buf, "topnviewerd") != NULL){
            ret = 1;
            break;
        }
    }
    if(ret == 0){
        QMessageBox MsgBox;
        MsgBox.setWindowTitle("Error");
        MsgBox.setText("Server is not running");
        MsgBox.setStandardButtons(QMessageBox::Ok);
        MsgBox.setDefaultButton(QMessageBox::Ok);
        if ( MsgBox.exec() == QMessageBox::Ok ){
            exit(1);
        }
    }
    sleep(1);
    int server_port = 2345;
    if(!connect_sock(&client_sock, server_port))
    {
        QMessageBox MsgBox;
        MsgBox.setWindowTitle("Socket failed");
        MsgBox.setText("Socket creation failed. plz restart.");
        MsgBox.setStandardButtons(QMessageBox::Ok);
        MsgBox.setDefaultButton(QMessageBox::Ok);
        if ( MsgBox.exec() == QMessageBox::Ok ){
            system("su -c \"pkill topnviewerd\"");
            exit(1);
        }
        exit(1);
    }

    scanThread_ = new ScanThread(client_sock);

    QObject::connect(scanThread_, &ScanThread::captured, this, &MainWindow::processCaptured, Qt::BlockingQueuedConnection);
    scanThread_->start();

    QStringList label = {"SSID", "Mac", ""};
    ui->tableWidget->setRowCount(0);
    ui->tableWidget->setHorizontalHeaderLabels(label);

    ui->tableWidget->setColumnWidth(1, 350);
    ui->tableWidget->setColumnWidth(2, 100);
}

MainWindow::~MainWindow()
{
    memset(buf, 0x00, BUF_SIZE);
    memcpy(buf, "7", 1);
    send_data(client_sock, buf);
    system("su -c \"pkill topnviewerd\"");
    delete ui;
}

void MainWindow::graphClicked(){
    QList<QCPAbstractPlottable *> graph_list = ui->graphWidget->selectedPlottables();
    if(graph_list.isEmpty()){
        return;
    }
    ui->label->setText(QString(graph_list[0]->name()));
}

template<template <typename> class P = std::less >
struct compare_pair_second {
    template<class T1, class T2> bool operator()(const std::pair<T1, T2>&left, const std::pair<T1, T2>&right) {
        return P<T2>()(left.second, right.second);
    }
};

void MainWindow::graphInit(){
    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%h:%m:%s");
    ui->graphWidget->xAxis->setTicker(timeTicker);
    ui->graphWidget->axisRect()->setupFullAxesBox();
    ui->graphWidget->yAxis->setRange(-90, 0);
    ui->graphWidget->setInteractions(QCP::iRangeDrag | QCP::iSelectPlottables);

    // make left and bottom axes transfer their ranges to right and top axes:
    connect(ui->graphWidget->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->graphWidget->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->graphWidget->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->graphWidget->yAxis2, SLOT(setRange(QCPRange)));
}

void MainWindow::RealtimeDataSlot()
{
    std::vector<std::pair<QString, int>> talker_vec;

    // copy map to vector for align
    ap_map[selected_ap].station_map.m.lock();
    for (auto iter = ap_map[selected_ap].station_map.begin(); iter != ap_map[selected_ap].station_map.end(); ++iter){
        if((clock() - iter.value().last_recv_time)/CLOCKS_PER_SEC > 8){
            for(int j = 0; j < ui->graphWidget->graphCount(); j++){
                if(ui->graphWidget->graph(j)->name() == iter.key()){
                    ui->graphWidget->removeGraph(j);
                    break;
                }
            }
            continue;
        }
        talker_vec.emplace_back(std::make_pair(iter.key(), iter.value().channel.toInt()));
    }
    ap_map[selected_ap].station_map.m.unlock();

    // sort by signal strength
    std::sort(talker_vec.begin(), talker_vec.end(), compare_pair_second<std::less>());

    if(talker_vec.size() > 5){
        talker_vec.erase(talker_vec.begin()+5, talker_vec.end()); // always less then 5
    }
    static QTime time(QTime::currentTime());
    qsrand(QTime::currentTime().msecsSinceStartOfDay());

    // calculate two new data points
    double key = time.elapsed()/1000.0; // time elapsed since start of demo, in seconds

    int graph_cnt = ui->graphWidget->graphCount();
    if((graph_cnt == 0) && (talker_vec.size() != 0)){ // first time
        for(auto iter = talker_vec.begin(); iter != talker_vec.end(); ++iter){

            graph_cnt = ui->graphWidget->graphCount();

            ui->graphWidget->addGraph();
            QPen color(makeColor(iter->first));
            color.setWidth(30);
            ui->graphWidget->graph(graph_cnt)->setPen(color);
            ui->graphWidget->graph(graph_cnt)->addData(key, iter->second);
            ui->graphWidget->graph(graph_cnt)->setName(iter->first);
        }
    }
    else if(graph_cnt != 0) { // not first time

        bool check_vector = false;
        bool check_graph = false;

        for(auto iter = talker_vec.begin(); iter != talker_vec.end(); ++iter){
            check_vector = false;
            for(int j = 0; j < ui->graphWidget->graphCount(); j++){
                if(ui->graphWidget->graph(j)->name() == iter->first){ // existing node
                    ui->graphWidget->graph(j)->addData(key, iter->second);
                    check_vector = true;
                    break;
                }
            }
            if(!check_vector){ // new node
                graph_cnt = ui->graphWidget->graphCount();
                for(int j = 0; j < talker_vec.size(); j++){
                    check_graph = false;
                    if(graph_cnt < 5){ // add node
                        QPen color(makeColor(iter->first));
                        color.setWidth(30);
                        ui->graphWidget->addGraph();
                        ui->graphWidget->graph(graph_cnt)->setPen(color);
                        ui->graphWidget->graph(graph_cnt)->addData(key, iter->second);
                        ui->graphWidget->graph(graph_cnt)->setName(iter->first);
                    }
                    else{
                        for(auto iter2 = talker_vec.begin(); iter2 != talker_vec.end(); ++iter2){ // node search to delete (j)
                            if(ui->graphWidget->graph(j)->name() == iter2->first){
                                check_graph = true;
                                break;
                            }
                        }
                        if(!check_graph){ // delete node j in graph
                            QPen color(makeColor(iter->first));
                            ui->graphWidget->addGraph();
                            int index = ui->graphWidget->graphCount();
                            color.setWidth(30);
                            ui->graphWidget->graph(index-1)->setPen(color);
                            ui->graphWidget->graph(index-1)->addData(key, iter->second);
                            ui->graphWidget->graph(index-1)->setName(iter->first);
                            ui->graphWidget->removeGraph(j);
                        }
                    }
                }
            }
        }
    }

    ui->graphWidget->xAxis->setRange(key,8,Qt::AlignRight);
    ui->graphWidget->replot();
}

void MainWindow::processCaptured(char* data)
{
    char temp_data[BUF_SIZE];
    memcpy(temp_data, data, BUF_SIZE);
    QString temp = temp_data;
    QStringList info = temp.split("\t");
    if((info.length() != 4) && (info.length() != 5))
    {
        return;
    }

    if((info[0] == "1") && (isApScan == true)) // if ap info
    {
        if(info[4].toInt() < -255){
            return;
        }

        int row = ui->tableWidget->rowCount();

        if (ap_map.find(info[2]) != ap_map.end())
        {
            for(int i=0; i<row; i++)
            {
                if(ui->tableWidget->item(i, 1)->text() == info[2])
                {
                    ui->tableWidget->item(i, 2)->setText(info[4]);
                    break;
                }
            }
            return;
        }

        // 1\tgoka_5g\t12:34:56\tchannel\tantsignal
        ap_map[info[2]].channel = info[3].toInt();

        ui->tableWidget->insertRow(row);
        QTableWidgetItem *item = new QTableWidgetItem(info[1]);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 0, item);
        QTableWidgetItem *item2 = new QTableWidgetItem(info[2]);
        item2->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item2->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 1, item2);
        QTableWidgetItem *item3 = new QTableWidgetItem(info[4]);
        item2->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item2->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 2, item3);

    }
    else if (info[0] == "2")
    {
        ap_map[info[1]].station_map.m.lock();
        ap_map[info[1]].station_map[info[2]].channel = info[3];
        ap_map[info[1]].station_map[info[2]].last_recv_time = clock();
        ap_map[info[1]].station_map.m.unlock();


        int ap_row = -1;

        for (int i=0; i<ui->tableWidget->rowCount(); i++)
        {
            if(ui->tableWidget->item(i, 1)->text() == info[1])
            {
                ap_row = i;
                break;
            }
        }

        if(ap_row == -1)
        {
            return;
        }

        ui->tableWidget->item(ap_row, 0)->setBackgroundColor(Qt::gray);
        ui->tableWidget->item(ap_row, 1)->setBackgroundColor(Qt::gray);
        ui->tableWidget->item(ap_row, 2)->setBackgroundColor(Qt::gray);
    }

}

void MainWindow::on_tableWidget_cellDoubleClicked(int row, int column)
{
    if(ui->tableWidget->item(row, 0)->backgroundColor() == Qt::gray){
        graphInit();
        usleep(5000);

        // setup a timer that repeatedly calls MainWindow::realtimeDataSlot:
        connect(&DataTimer, SIGNAL(timeout()), this, SLOT(RealtimeDataSlot()));
        connect(ui->graphWidget, SIGNAL(mouseDoubleClick(QMouseEvent*)), this, SLOT(graphClicked()));

        selected_ap = ui->tableWidget->item(row, 1)->text();

        memset(buf, 0x00, BUF_SIZE);
        memcpy(buf, "4", 1);
        send_data(client_sock, buf);

        memset(buf, 0x00, BUF_SIZE);
        strcpy(buf, QString::number(ap_map[selected_ap].channel).toStdString().c_str());
        send_data(client_sock, buf);

        memset(buf, 0x00, BUF_SIZE);
        strcpy(buf, QString(selected_ap).toStdString().c_str());
        send_data(client_sock, buf);

        DataTimer.start(1000);
    }
}

void MainWindow::on_pushButton_clicked()
{
    if(ui->pushButton->text() == "AP scan start"){ // Not start
        ui->pushButton->setText("AP scan stop");
        ui->label->setText("");

        DataTimer.stop();

        ui->graphWidget->clearGraphs();
        ui->graphWidget->replot();

        for(auto it = ap_map.begin(); it != ap_map.end(); it++)
        {
            it->second.station_map.clear();
        }

        for(int i = 0; i < ui->tableWidget->rowCount(); i++){
            ui->tableWidget->item(i, 0)->setBackgroundColor(Qt::white);
            ui->tableWidget->item(i, 1)->setBackgroundColor(Qt::white);
            ui->tableWidget->item(i, 2)->setBackgroundColor(Qt::white);
        }

        isApScan = true;
        memset(buf, 0x00, BUF_SIZE);
        memcpy(buf, "2", 1);
        send_data(client_sock, buf);

    }else{ // stop ap scan
        ui->pushButton->setText("AP scan start");
        isApScan = false;
        memset(buf, 0x00, BUF_SIZE);
        memcpy(buf, "3", 1);
        send_data(client_sock, buf);
    }
}
