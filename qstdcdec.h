#ifndef QSTDCDEC_H
#define QSTDCDEC_H

#include <QMainWindow>
#include <QTableWidgetItem>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QMessageBox>
#include <QMetaEnum>
#include <QColor>
#include <QScrollBar>
#include <QSettings>

#include <thread>
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <array>
#include <fstream>

#include "json.hpp"

#include <inmarsatc_parser.h>

QT_BEGIN_NAMESPACE
namespace Ui { class QStdcdec; }
QT_END_NAMESPACE

class QStdcdec : public QMainWindow {
    Q_OBJECT

public:
    QStdcdec(QWidget *parent = nullptr);
    ~QStdcdec();

private slots:
    void on_tableWidget_itemSelectionChanged();
    void on_tableWidget_2_itemSelectionChanged();
    void on_pushButton_toggled(bool checked);
    void on_pushButton_3_clicked();
    void on_pushButton_2_clicked();
    void on_checkBox_stateChanged(int arg1);
    void on_checkBox_3_stateChanged(int arg1);
    void on_pushButton_4_clicked();
    void on_pushButton_5_clicked();
    void on_spinBox_valueChanged(int arg1);
    void on_checkBox_2_stateChanged(int arg1);
    void on_lineEdit_textChanged(const QString &arg1);
    void on_checkBox_4_stateChanged(int arg1);
    void on_lineEdit_3_textChanged(const QString &arg1);

    void socketErrorOccurred(QAbstractSocket::SocketError socketError);
    void socketDataReady();

private:
    struct Message {
        std::chrono::time_point<std::chrono::high_resolution_clock> timestamp;
        std::string sat;
        std::string les;
        std::string lcn;
        int representation;
        bool complete;
        bool fromBeginning;
        int packet_cnt;
        //std::vector<inmarsatc::frameParser::FrameParser::frameParser_result> packets;
        std::string msg;
        std::string priority;
    };
    Ui::QStdcdec *ui;
    inmarsatc::frameParser::FrameParser parser;
    std::vector<inmarsatc::frameParser::FrameParser::frameParser_result> packets;
    std::vector<Message> messages;
    std::vector<int> uncomplete_messages;
    QUdpSocket receiverSocket;
    QSettings settings;
    bool settingsLoaded = false;

    void reloadPacketsTableFromArray();
    void appendPacketsTableAndArray(inmarsatc::frameParser::FrameParser::frameParser_result packet);
    void reloadMessagesTableFromArray();
    void appendMessagesTableAndArray(Message m);
    void updateMessagesTableAndArray(Message m, int index);
    void savePacketsToFile();
    void loadPacketsFromFile();
    void saveMessagesToFile();
    void loadMessagesFromFile();
    void saveConfigToFile();
    void loadConfigFromFile();
    void processPacketAsMessage(inmarsatc::frameParser::FrameParser::frameParser_result packet);
    void updateDetailedPacket(inmarsatc::frameParser::FrameParser::frameParser_result packet);
    void updateMessage(Message m);
    void processFrame(inmarsatc::decoder::Decoder::decoder_result frame);
};
#endif // QSTDCDEC_H
