#include "qstdcdec.h"
#include "./ui_qstdcdec.h"

QStdcdec::QStdcdec(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::QStdcdec),
    settings("Indir", "QStdcdec") {
    ui->setupUi(this);
    ui->centralwidget->setLayout(ui->gridLayout);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    ui->tableWidget_2->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    loadConfigFromFile();
    if(ui->checkBox_2->isChecked()) {
        loadPacketsFromFile();
    }
    if(ui->checkBox_4->isChecked()) {
        loadMessagesFromFile();
    }
    connect(&receiverSocket, &QUdpSocket::readyRead, this, &QStdcdec::socketDataReady);
    connect(&receiverSocket, &QUdpSocket::errorOccurred, this, &QStdcdec::socketErrorOccurred);
}

QStdcdec::~QStdcdec() {
    delete ui;
}

//serialization
template< typename T >
std::array< char, sizeof(T) >  to_bytes( const T& object ) {
    static_assert( std::is_trivially_copyable<T>::value, "not a TriviallyCopyable type" ) ;
    std::array< char, sizeof(T) > bytes ;

    const char* begin = reinterpret_cast< const char* >( std::addressof(object) ) ;
    const char* end = begin + sizeof(T) ;
    std::copy( begin, end, std::begin(bytes) ) ;

    return bytes ;
}

template< typename T >
T& from_bytes( const QByteArray& bytes, T& object ) {
    // http://en.cppreference.com/w/cpp/types/is_trivially_copyable
    static_assert( std::is_trivially_copyable<T>::value, "not a TriviallyCopyable type" ) ;

    char* begin_object = reinterpret_cast< char* >( std::addressof(object) ) ;
    std::copy( std::begin(bytes), std::end(bytes), begin_object ) ;

    return object ;
}

bool compareQTWSelRanges(QTableWidgetSelectionRange a, QTableWidgetSelectionRange b) {
    return (a.bottomRow() == b.bottomRow()) && (a.leftColumn() == b.leftColumn()) && (a.topRow() == b.topRow()) && (a.rightColumn() == b.rightColumn());
    // return: a == b
}

bool ifPacketIsMessage(inmarsatc::frameParser::FrameParser::frameParser_result pack_dec_res) {
    switch(pack_dec_res.decoding_result.packetDescriptor) {
//        case 0xA3:
//        case 0xA8:
//            return pack_dec_res.decoding_result.packetVars.find("shortMessage") != pack_dec_res.decoding_result.packetVars.end();
        case 0xAA:
        case 0xB1:
        case 0xB2:
            return true;
        default:
            return false;
    }
}

void QStdcdec::on_tableWidget_itemSelectionChanged() {
    bool oldState = ui->tableWidget->blockSignals(true);
    if(ui->tableWidget->selectedRanges().size() > 0) {
        QTableWidgetSelectionRange wsr = this->ui->tableWidget->selectedRanges().at(0);
        QTableWidgetSelectionRange fin(wsr.topRow(), 0, wsr.topRow(), this->ui->tableWidget->columnCount() - 1);
        if(this->ui->tableWidget->selectedRanges().size() == 1 && compareQTWSelRanges(wsr, fin)) {
            goto end;
        }
        for(int i = 0; i < this->ui->tableWidget->selectedRanges().size(); i++) {
            this->ui->tableWidget->setRangeSelected(this->ui->tableWidget->selectedRanges().at(i), false);
        }
        this->ui->tableWidget->setRangeSelected(fin, true);
        updateDetailedPacket(packets[fin.topRow()]);
    }
end:
    ui->tableWidget->blockSignals(oldState);
}

void QStdcdec::on_tableWidget_2_itemSelectionChanged() {
    bool oldState = ui->tableWidget_2->blockSignals(true);
    if(ui->tableWidget_2->selectedRanges().size() > 0) {
        QTableWidgetSelectionRange wsr = this->ui->tableWidget_2->selectedRanges().at(0);
        QTableWidgetSelectionRange fin(wsr.topRow(), 0, wsr.topRow(), this->ui->tableWidget_2->columnCount() - 1);
        if(this->ui->tableWidget_2->selectedRanges().size() == 1 && compareQTWSelRanges(wsr, fin)) {
            goto end;
        }
        for(int i = 0; i < this->ui->tableWidget_2->selectedRanges().size(); i++) {
            this->ui->tableWidget_2->setRangeSelected(this->ui->tableWidget_2->selectedRanges().at(i), false);
        }
        this->ui->tableWidget_2->setRangeSelected(fin, true);
        updateMessage(messages[fin.topRow()]);
    } else {
        ui->listWidget->clear();
    }
end:
    ui->tableWidget_2->blockSignals(oldState);
}

void QStdcdec::on_pushButton_toggled(bool checked) {
    if(checked) {
        if(receiverSocket.bind(ui->spinBox->value())) {
            ui->spinBox->setEnabled(false);
            this->ui->pushButton->setText("Stop listening");
        }
    } else {
        receiverSocket.close();
        ui->spinBox->setEnabled(true);
        this->ui->pushButton->setText("Listen");
    }
    saveConfigToFile();
}

void QStdcdec::socketErrorOccurred(QAbstractSocket::SocketError socketError) {
    this->ui->pushButton->setChecked(false);
    ui->spinBox->setEnabled(true);
    this->ui->pushButton->setText("Listen");
    QMetaEnum metaEnum = QMetaEnum::fromType<QAbstractSocket::SocketError>();
    QMessageBox messageBox;
    messageBox.critical(ui->centralwidget, "Socket error", (QString("Socket error: ") + QString(metaEnum.valueToKey(socketError))));
    messageBox.setFixedSize(500,200);
    std::cerr << "Socket error: " << metaEnum.valueToKey(socketError) << "(" << socketError << ")" << std::endl;
    saveConfigToFile();
}

void QStdcdec::socketDataReady() {
    while(receiverSocket.hasPendingDatagrams()) {
        QNetworkDatagram datagram = receiverSocket.receiveDatagram();
        QByteArray data = datagram.data();
        if(data.length() != sizeof(inmarsatc::decoder::Decoder::decoder_result)) continue;
        inmarsatc::decoder::Decoder::decoder_result ret;
        from_bytes(data, ret);
        processFrame(ret);
    }
}

void QStdcdec::reloadPacketsTableFromArray() {
    ui->tableWidget->clearContents();
    ui->tableWidget->setRowCount(packets.size());
    for(int i = 0; i < packets.size(); i++) {
        inmarsatc::frameParser::FrameParser::frameParser_result curr_packet = packets[i];
        QTableWidgetItem* fn = new QTableWidgetItem(QString::number(curr_packet.decoding_result.frameNumber));
        QTableWidgetItem* msg = new QTableWidgetItem(ifPacketIsMessage(curr_packet) ? "Y" : "");
        msg->setForeground(QBrush(QColor::fromRgb(0, 255, 0, 255), Qt::SolidPattern));
        QTableWidgetItem* type = new QTableWidgetItem("0x" + QString::number(curr_packet.decoding_result.packetDescriptor, 16) + "  | " + QString::fromStdString(curr_packet.decoding_result.packetVars["packetDescriptorText"]));
        fn->setFlags(fn->flags() ^ Qt::ItemIsEditable);
        msg->setFlags(msg->flags() ^ Qt::ItemIsEditable);
        type->setFlags(type->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 0, fn);
        ui->tableWidget->setItem(i, 1, msg);
        ui->tableWidget->setItem(i, 2, type);
    }
    ui->tableWidget->scrollToBottom();
    if(ui->checkBox->isChecked()) {
        savePacketsToFile();
    }
}

void QStdcdec::appendPacketsTableAndArray(inmarsatc::frameParser::FrameParser::frameParser_result packet) {
    packets.push_back(packet);
    ui->tableWidget->insertRow(ui->tableWidget->rowCount());
    QTableWidgetItem* fn = new QTableWidgetItem(QString::number(packet.decoding_result.frameNumber));
    QTableWidgetItem* msg = new QTableWidgetItem(ifPacketIsMessage(packet) ? "Y" : "");
    msg->setForeground(QBrush(QColor::fromRgb(0, 255, 0, 255), Qt::SolidPattern));
    QTableWidgetItem* type = new QTableWidgetItem("0x" + QString::number(packet.decoding_result.packetDescriptor, 16) + "  | " + QString::fromStdString(packet.decoding_result.packetVars["packetDescriptorText"]));
    fn->setFlags(fn->flags() ^ Qt::ItemIsEditable);
    msg->setFlags(msg->flags() ^ Qt::ItemIsEditable);
    type->setFlags(type->flags() ^ Qt::ItemIsEditable);
    ui->tableWidget->setItem(ui->tableWidget->rowCount()-1, 0, fn);
    ui->tableWidget->setItem(ui->tableWidget->rowCount()-1, 1, msg);
    ui->tableWidget->setItem(ui->tableWidget->rowCount()-1, 2, type);
    if(ui->tableWidget->verticalScrollBar()->value() == ui->tableWidget->verticalScrollBar()->maximum()) {
        ui->tableWidget->scrollToBottom();
    }
    if(ui->checkBox->isChecked()) {
        savePacketsToFile();
    }
}

void QStdcdec::reloadMessagesTableFromArray() {
    ui->tableWidget_2->clearContents();
    ui->tableWidget_2->setRowCount(messages.size());
    for(int i = 0; i < messages.size(); i++) {
        Message curr_msg = messages[i];
        const time_t x = std::chrono::system_clock::to_time_t(curr_msg.timestamp);
        QTableWidgetItem* ts = new QTableWidgetItem(QString(std::ctime(&x)));
        QTableWidgetItem* sat = new QTableWidgetItem(QString::fromStdString(curr_msg.sat));
        QTableWidgetItem* les = new QTableWidgetItem(QString::fromStdString(curr_msg.les));
        QTableWidgetItem* lcn = new QTableWidgetItem(QString::fromStdString(curr_msg.lcn));
        QTableWidgetItem* type;
        if(curr_msg.representation == PACKETDECODER_PRESENTATION_IA5) {
            type = new QTableWidgetItem("IA5");
        } else if(curr_msg.representation == PACKETDECODER_PRESENTATION_ITA2) {
            type = new QTableWidgetItem("ITA2");
        } else {
            type = new QTableWidgetItem("Bin");
        }
        QTableWidgetItem* complete = new QTableWidgetItem((curr_msg.complete && curr_msg.fromBeginning) ? "Y" : "");
        complete->setForeground(QBrush(QColor::fromRgb(0, 255, 0, 255), Qt::SolidPattern));
        QTableWidgetItem* pcnt = new QTableWidgetItem(QString::number(curr_msg.packet_cnt));
        QTableWidgetItem* priority = new QTableWidgetItem(QString::fromStdString(curr_msg.priority));
        ts->setFlags(ts->flags() ^ Qt::ItemIsEditable);
        sat->setFlags(sat->flags() ^ Qt::ItemIsEditable);
        les->setFlags(les->flags() ^ Qt::ItemIsEditable);
        lcn->setFlags(lcn->flags() ^ Qt::ItemIsEditable);
        type->setFlags(type->flags() ^ Qt::ItemIsEditable);
        complete->setFlags(complete->flags() ^ Qt::ItemIsEditable);
        pcnt->setFlags(pcnt->flags() ^ Qt::ItemIsEditable);
        priority->setFlags(priority->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget_2->setItem(i, 0, ts);
        ui->tableWidget_2->setItem(i, 1, sat);
        ui->tableWidget_2->setItem(i, 2, les);
        ui->tableWidget_2->setItem(i, 3, lcn);
        ui->tableWidget_2->setItem(i, 4, type);
        ui->tableWidget_2->setItem(i, 5, complete);
        ui->tableWidget_2->setItem(i, 6, pcnt);
        ui->tableWidget_2->setItem(i, 7, priority);
    }
    ui->tableWidget_2->scrollToBottom();
    if(ui->checkBox_3->isChecked()) {
        saveMessagesToFile();
    }
}

void QStdcdec::appendMessagesTableAndArray(Message m) {
    messages.push_back(m);
    if(!m.complete) {
        uncomplete_messages.push_back(messages.size()-1);
    }
    ui->tableWidget_2->insertRow(ui->tableWidget_2->rowCount());
    const time_t x = std::chrono::system_clock::to_time_t(m.timestamp);
    QTableWidgetItem* ts = new QTableWidgetItem(QString(std::ctime(&x)));
    QTableWidgetItem* sat = new QTableWidgetItem(QString::fromStdString(m.sat));
    QTableWidgetItem* les = new QTableWidgetItem(QString::fromStdString(m.les));
    QTableWidgetItem* lcn = new QTableWidgetItem(QString::fromStdString(m.lcn));
    QTableWidgetItem* type;
    if(m.representation == PACKETDECODER_PRESENTATION_IA5) {
        type = new QTableWidgetItem("IA5");
    } else if(m.representation == PACKETDECODER_PRESENTATION_ITA2) {
        type = new QTableWidgetItem("ITA2");
    } else {
        type = new QTableWidgetItem("Bin");
    }
    QTableWidgetItem* complete = new QTableWidgetItem((m.complete && m.fromBeginning) ? "Y" : "");
    complete->setForeground(QBrush(QColor::fromRgb(0, 255, 0, 255), Qt::SolidPattern));
    QTableWidgetItem* pcnt = new QTableWidgetItem(QString::number(m.packet_cnt));
    QTableWidgetItem* priority = new QTableWidgetItem(QString::fromStdString(m.priority));
    ts->setFlags(ts->flags() ^ Qt::ItemIsEditable);
    sat->setFlags(sat->flags() ^ Qt::ItemIsEditable);
    les->setFlags(les->flags() ^ Qt::ItemIsEditable);
    lcn->setFlags(lcn->flags() ^ Qt::ItemIsEditable);
    type->setFlags(type->flags() ^ Qt::ItemIsEditable);
    complete->setFlags(complete->flags() ^ Qt::ItemIsEditable);
    pcnt->setFlags(pcnt->flags() ^ Qt::ItemIsEditable);
    priority->setFlags(priority->flags() ^ Qt::ItemIsEditable);
    ui->tableWidget_2->setItem(ui->tableWidget_2->rowCount()-1, 0, ts);
    ui->tableWidget_2->setItem(ui->tableWidget_2->rowCount()-1, 1, sat);
    ui->tableWidget_2->setItem(ui->tableWidget_2->rowCount()-1, 2, les);
    ui->tableWidget_2->setItem(ui->tableWidget_2->rowCount()-1, 3, lcn);
    ui->tableWidget_2->setItem(ui->tableWidget_2->rowCount()-1, 4, type);
    ui->tableWidget_2->setItem(ui->tableWidget_2->rowCount()-1, 5, complete);
    ui->tableWidget_2->setItem(ui->tableWidget_2->rowCount()-1, 6, pcnt);
    ui->tableWidget_2->setItem(ui->tableWidget_2->rowCount()-1, 7, priority);
    if(ui->tableWidget_2->verticalScrollBar()->value() == ui->tableWidget_2->verticalScrollBar()->maximum()) {
        ui->tableWidget_2->scrollToBottom();
    }
    if(ui->checkBox_3->isChecked()) {
        saveMessagesToFile();
    }
}

void QStdcdec::updateMessagesTableAndArray(Message m, int index) {
    messages[index] = m;
    //removing from uncomplete_messages on next processPacketAsMessage() call
    const time_t x = std::chrono::system_clock::to_time_t(m.timestamp);
    QTableWidgetItem* ts = new QTableWidgetItem(QString(std::ctime(&x)));
    QTableWidgetItem* sat = new QTableWidgetItem(QString::fromStdString(m.sat));
    QTableWidgetItem* les = new QTableWidgetItem(QString::fromStdString(m.les));
    QTableWidgetItem* lcn = new QTableWidgetItem(QString::fromStdString(m.lcn));
    QTableWidgetItem* type;
    if(m.representation == PACKETDECODER_PRESENTATION_IA5) {
        type = new QTableWidgetItem("IA5");
    } else if(m.representation == PACKETDECODER_PRESENTATION_ITA2) {
        type = new QTableWidgetItem("ITA2");
    } else {
        type = new QTableWidgetItem("Bin");
    }
    QTableWidgetItem* complete = new QTableWidgetItem((m.complete && m.fromBeginning) ? "Y" : "");
    complete->setForeground(QBrush(QColor::fromRgb(0, 255, 0, 255), Qt::SolidPattern));
    QTableWidgetItem* pcnt = new QTableWidgetItem(QString::number(m.packet_cnt));
    QTableWidgetItem* priority = new QTableWidgetItem(QString::fromStdString(m.priority));
    ts->setFlags(ts->flags() ^ Qt::ItemIsEditable);
    sat->setFlags(sat->flags() ^ Qt::ItemIsEditable);
    les->setFlags(les->flags() ^ Qt::ItemIsEditable);
    lcn->setFlags(lcn->flags() ^ Qt::ItemIsEditable);
    type->setFlags(type->flags() ^ Qt::ItemIsEditable);
    complete->setFlags(complete->flags() ^ Qt::ItemIsEditable);
    pcnt->setFlags(pcnt->flags() ^ Qt::ItemIsEditable);
    priority->setFlags(priority->flags() ^ Qt::ItemIsEditable);
    ui->tableWidget_2->setItem(index, 0, ts);
    ui->tableWidget_2->setItem(index, 1, sat);
    ui->tableWidget_2->setItem(index, 2, les);
    ui->tableWidget_2->setItem(index, 3, lcn);
    ui->tableWidget_2->setItem(index, 4, type);
    ui->tableWidget_2->setItem(index, 5, complete);
    ui->tableWidget_2->setItem(index, 6, pcnt);
    ui->tableWidget_2->setItem(index, 7, priority);
    on_tableWidget_2_itemSelectionChanged();
    if(ui->checkBox_3->isChecked()) {
        saveMessagesToFile();
    }
}

void QStdcdec::savePacketsToFile() {
    nlohmann::json outj = nlohmann::json::array();
    for(int i = 0; i < packets.size(); i++) {
        nlohmann::json j;
        j["frameNumber"] = packets[i].decoding_result.frameNumber;
        j["timestamp"] = std::chrono::high_resolution_clock::to_time_t(packets[i].decoding_result.timestamp);
        j["packetDescriptor"] = packets[i].decoding_result.packetDescriptor;
        j["packetLength"] = packets[i].decoding_result.packetLength;
        j["decodingStage"] = packets[i].decoding_result.decodingStage;
        j["payload"]["presentation"] = packets[i].decoding_result.payload.presentation;
        j["payload"]["data"] = packets[i].decoding_result.payload.data8Bit;
        j["packetVars"] = packets[i].decoding_result.packetVars;
        outj.push_back(j);
    }
    std::string data = outj.dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore);
    QString path = ui->lineEdit->text();
    std::ofstream ofile(path.toUtf8().constData(), std::ios::binary);
    ofile.write(data.data(), data.size());
    ofile.close();
    if(!ofile) {
        QMessageBox messageBox;
        messageBox.critical(ui->centralwidget, "File writing error", "Writing to the file failed: " + QString(strerror(errno)));
        messageBox.setFixedSize(500,200);
        std::cerr << "Writing to file failed: " << strerror(errno) << std::endl;
        ui->checkBox->setChecked(false);
    }
}

void QStdcdec::loadPacketsFromFile() {
    QString path = ui->lineEdit->text();
    std::ifstream ifile(path.toUtf8().constData(), std::ios::binary);
    nlohmann::json data = nlohmann::json::parse(ifile, nullptr, false);
    if(!data.is_discarded()) {
        packets.clear();
        for(auto it = data.begin(); it != data.end(); it++) {
            nlohmann::json el = *it;
            inmarsatc::frameParser::FrameParser::frameParser_result res;
            res.decoding_result.frameNumber = el["frameNumber"];
            res.decoding_result.timestamp = std::chrono::high_resolution_clock::from_time_t(el["timestamp"]);
            res.decoding_result.packetDescriptor = el["packetDescriptor"];
            res.decoding_result.packetLength = el["packetLength"];
            res.decoding_result.decodingStage = el["decodingStage"];
            res.decoding_result.payload.presentation = el["payload"]["presentation"];
            res.decoding_result.payload.data8Bit = el["payload"]["data"].get<std::vector<uint8_t>>();
            res.decoding_result.packetVars = el["packetVars"].get<std::map<std::string, std::string>>();
            res.decoding_result.isDecodedPacket = true;
            res.decoding_result.isCrc = true;
            packets.push_back(res);
        }
        reloadPacketsTableFromArray();
    } else {
        QMessageBox messageBox;
        messageBox.critical(ui->centralwidget, "File reading error", "Reading from the file failed");
        messageBox.setFixedSize(500,200);
        std::cerr << "Reading from the file failed " << std::endl;
    }
    ifile.close();
}

void QStdcdec::saveMessagesToFile() {
    nlohmann::json outj = nlohmann::json::array();
    for(int i = 0; i < messages.size(); i++) {
        nlohmann::json j;
        j["timestamp"] = std::chrono::high_resolution_clock::to_time_t(messages[i].timestamp);
        j["sat"] = messages[i].sat;
        j["les"] = messages[i].les;
        j["lcn"] = messages[i].lcn;
        j["repr"] = messages[i].representation;
        j["complete"] = messages[i].complete;
        j["fromBeginning"] = messages[i].fromBeginning;
        j["packetCnt"] = messages[i].packet_cnt;
        j["msg"] = messages[i].msg;
        j["priority"] = messages[i].priority;
        outj.push_back(j);
    }
    std::string data = outj.dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore);
    QString path = ui->lineEdit_3->text();
    std::ofstream ofile(path.toUtf8().constData(), std::ios::binary);
    ofile.write(data.data(), data.size());
    ofile.close();
    if(!ofile) {
        QMessageBox messageBox;
        messageBox.critical(ui->centralwidget, "File writing error", "Writing to the file failed");
        messageBox.setFixedSize(500,200);
        std::cerr << "Writing to file failed " << std::endl;
        ui->checkBox->setChecked(false);
    }
}

void QStdcdec::loadMessagesFromFile() {
    QString path = ui->lineEdit_3->text();
    std::ifstream ifile(path.toUtf8().constData(), std::ios::binary);
    nlohmann::json data = nlohmann::json::parse(ifile, nullptr, false);
    if(!data.is_discarded()) {
        messages.clear();
        for(auto it = data.begin(); it != data.end(); it++) {
            nlohmann::json el = *it;
            Message ret;
            ret.timestamp = std::chrono::high_resolution_clock::from_time_t(el["timestamp"]);
            ret.sat = el["sat"];
            ret.les = el["les"];
            ret.lcn = el["lcn"];
            ret.representation = el["repr"];
            ret.complete = el["complete"];
            ret.fromBeginning = el["fromBeginning"];
            ret.packet_cnt = el["packetCnt"];
            ret.msg = el["msg"];
            ret.priority = el["priority"];
            messages.push_back(ret);
        }
        reloadMessagesTableFromArray();
    } else {
        QMessageBox messageBox;
        messageBox.critical(ui->centralwidget, "File reading error", "Reading from the file failed");
        messageBox.setFixedSize(500,200);
        std::cerr << "Reading from the file failed " << std::endl;
    }
    ifile.close();
}

void QStdcdec::saveConfigToFile() {
    if(!settingsLoaded) {
        return;
    }
    settings.setValue("savepackets", ui->checkBox->isChecked() ? "true" : "false");
    settings.setValue("autoloadpackets", ui->checkBox_2->isChecked() ? "true" : "false");
    settings.setValue("packetspath", ui->lineEdit->text());
    settings.setValue("savemessages", ui->checkBox_3->isChecked() ? "true" : "false");
    settings.setValue("autoloadmessages", ui->checkBox_4->isChecked() ? "true" : "false");
    settings.setValue("messagespath", ui->lineEdit_3->text());
    settings.setValue("port", ui->spinBox->value());
    settings.setValue("listening", ui->pushButton->isChecked() ? "true" : "false");
    settings.sync();
}

void QStdcdec::loadConfigFromFile() {
    ui->checkBox->setChecked(settings.value("savepackets", "false") == "false" ? false : true);
    if(ui->checkBox->isChecked()) {
        ui->lineEdit->setEnabled(false);
    }
    ui->checkBox_2->setChecked(settings.value("autoloadpackets", "false") == "false" ? false : true);
    ui->lineEdit->setText(settings.value("packetspath", "/tmp/qstdcdec_packets.json").toString());
    ui->checkBox_3->setChecked(settings.value("savemessages", "false") == "false" ? false : true);
    if(ui->checkBox_3->isChecked()) {
        ui->lineEdit_3->setEnabled(false);
    }
    ui->checkBox_4->setChecked(settings.value("autoloadmessages", "false") == "false" ? false : true);
    ui->lineEdit_3->setText(settings.value("messagespath", "/tmp/qstdcdec_messages.json").toString());
    ui->spinBox->setValue(settings.value("port", "15004").toInt());
    if(settings.value("listening", "false") != "false") {
        ui->pushButton->toggle();
    }
    settingsLoaded = true;
}

void QStdcdec::processPacketAsMessage(inmarsatc::frameParser::FrameParser::frameParser_result packet) {
    if(ifPacketIsMessage(packet)) {
        std::string packet_sat = packet.decoding_result.packetVars["sat"];
        std::string packet_les = packet.decoding_result.packetVars["lesId"];
        std::string packet_lcnegc = (packet.decoding_result.packetDescriptor == 0xAA) ? packet.decoding_result.packetVars["logicalChannelNo"] : packet.decoding_result.packetVars["messageId"];
        int packet_num = std::atoi(packet.decoding_result.packetVars["packetNo"].c_str());
        if(packet_num != 0) {
            //Check, if there any uncomplete messages
            for(int i = 0; i < uncomplete_messages.size(); i++) {
                if(messages[uncomplete_messages[i]].complete) {
                    uncomplete_messages.erase(uncomplete_messages.begin()+i);
                    i = 0;
                    continue;
                }
                if(messages[uncomplete_messages[i]].les == packet_les && messages[uncomplete_messages[i]].sat == packet_sat && messages[uncomplete_messages[i]].lcn == packet_lcnegc) {
                    if(messages[uncomplete_messages[i]].packet_cnt < packet_num) {
                        Message m = messages[uncomplete_messages[i]];
                        m.msg += std::string(packet.decoding_result.payload.data8Bit.begin(), packet.decoding_result.payload.data8Bit.end());
                        m.packet_cnt = packet_num;
                        updateMessagesTableAndArray(m, i);
                        return;
                    } else {
                        Message m = messages[uncomplete_messages[i]];
                        m.complete = true;
                        updateMessagesTableAndArray(m, i);
                    }
                }
            }
        }
        Message m;
        m.complete = false;
        m.fromBeginning = packet_num == 0;
        m.packet_cnt = packet_num;
        m.lcn = packet_lcnegc;
        m.les = packet_les;
        m.sat = packet_sat;
        m.representation = packet.decoding_result.payload.presentation;
        m.timestamp = packet.decoding_result.timestamp;
        m.msg = std::string(packet.decoding_result.payload.data8Bit.begin(), packet.decoding_result.payload.data8Bit.end());
        (packet.decoding_result.packetDescriptor == 0xAA) ? m.priority = "" : m.priority = packet.decoding_result.packetVars["priorityText"];
        appendMessagesTableAndArray(m);
    }
}

void QStdcdec::updateDetailedPacket(inmarsatc::frameParser::FrameParser::frameParser_result packet) {
    ui->listWidget->clear();
    ui->listWidget->addItem("Type: " + QString::fromStdString(packet.decoding_result.packetVars["packetDescriptorText"]) + "(" + QString::number(packet.decoding_result.packetDescriptor, 16) + ")");
    ui->listWidget->addItem("Frame number: " + QString::number(packet.decoding_result.frameNumber));
    std::time_t timestamp = std::chrono::high_resolution_clock::to_time_t(packet.decoding_result.timestamp);
    ui->listWidget->addItem("Timestamp: " + QString(std::ctime(&timestamp)));
    ui->listWidget->addItem("Decoding stage: " + QString(packet.decoding_result.decodingStage == PACKETDECODER_DECODING_STAGE_NONE ? "none" : (packet.decoding_result.decodingStage == PACKETDECODER_DECODING_STAGE_COMPLETE ? "complete" : "partial")));
    ui->listWidget->addItem("Length: " + QString::number(packet.decoding_result.packetLength));

    switch(packet.decoding_result.packetDescriptor) {
        case 0x27:
            {
                ui->listWidget->addItem("Sat: " + QString::fromStdString(packet.decoding_result.packetVars["satName"]));
                ui->listWidget->addItem("LES: " + QString::fromStdString(packet.decoding_result.packetVars["lesName"]));
                ui->listWidget->addItem("LCN: " + QString::fromStdString(packet.decoding_result.packetVars["logicalChannelNo"]));
                ui->listWidget->addItem("Message ID: " + QString::fromStdString(packet.decoding_result.packetVars["mesId"]));
            }
            break;
        case 0x2A:
            {
                ui->listWidget->addItem("Sat: " + QString::fromStdString(packet.decoding_result.packetVars["satName"]));
                ui->listWidget->addItem("LES: " + QString::fromStdString(packet.decoding_result.packetVars["lesName"]));
                ui->listWidget->addItem("LCN: " + QString::fromStdString(packet.decoding_result.packetVars["logicalChannelNo"]));
                ui->listWidget->addItem("Message ID: " + QString::fromStdString(packet.decoding_result.packetVars["mesId"]));
                ui->listWidget->addItem("Unknown 1: " + QString::fromStdString(packet.decoding_result.packetVars["unknown1Hex"]));
            }
            break;
        case 0x08:
            {
                ui->listWidget->addItem("Sat: " + QString::fromStdString(packet.decoding_result.packetVars["satName"]));
                ui->listWidget->addItem("LES: " + QString::fromStdString(packet.decoding_result.packetVars["lesName"]));
                ui->listWidget->addItem("LCN: " + QString::fromStdString(packet.decoding_result.packetVars["logicalChannelNo"]));
                ui->listWidget->addItem("Uplink channel(MHz): " + QString::fromStdString(packet.decoding_result.packetVars["uplinkChannelMhz"]));
            }
            break;
        case 0x6C:
            {
                ui->listWidget->addItem("Uplink channel(MHz): " + QString::fromStdString(packet.decoding_result.packetVars["uplinkChannelMhz"]));
                ui->listWidget->addItem("Services: \n" + QString::fromStdString(packet.decoding_result.packetVars["services"]) + "\n");
                ui->listWidget->addItem("TDM Slots: \n" + QString::fromStdString(packet.decoding_result.packetVars["tdmSlots"]) + "\n");
            }
            break;
        case 0x7D:
            {
                ui->listWidget->addItem("Sat: " + QString::fromStdString(packet.decoding_result.packetVars["satName"]));
                ui->listWidget->addItem("LES: " + QString::fromStdString(packet.decoding_result.packetVars["lesName"]));
                ui->listWidget->addItem("Network version: " + QString::fromStdString(packet.decoding_result.packetVars["networkVersion"]));
                ui->listWidget->addItem("Signalling channel: " + QString::fromStdString(packet.decoding_result.packetVars["signallingChannel"]));
                ui->listWidget->addItem("Count: " + QString::fromStdString(packet.decoding_result.packetVars["count"]));
                ui->listWidget->addItem("Channel type: " + QString::fromStdString(packet.decoding_result.packetVars["channelTypeName"]) + "(" + QString::fromStdString(packet.decoding_result.packetVars["channelType"]) + ")");
                ui->listWidget->addItem("Local: " + QString::fromStdString(packet.decoding_result.packetVars["local"]));
                ui->listWidget->addItem("Random interval: " + QString::fromStdString(packet.decoding_result.packetVars["randomInterval"]));
                ui->listWidget->addItem("Status: \n" + QString::fromStdString(packet.decoding_result.packetVars["status"]) + "\n");
                ui->listWidget->addItem("Services: \n" + QString::fromStdString(packet.decoding_result.packetVars["services"]) + "\n");
            }
            break;
        case 0x81:
            {
                ui->listWidget->addItem("Sat: " + QString::fromStdString(packet.decoding_result.packetVars["satName"]));
                ui->listWidget->addItem("LES: " + QString::fromStdString(packet.decoding_result.packetVars["lesName"]));
                ui->listWidget->addItem("LCN: " + QString::fromStdString(packet.decoding_result.packetVars["logicalChannelNo"]));
                ui->listWidget->addItem("Message ID: " + QString::fromStdString(packet.decoding_result.packetVars["mesId"]));
                ui->listWidget->addItem("Downlink channel(MHz): " + QString::fromStdString(packet.decoding_result.packetVars["downlinkChannelMhz"]));
                ui->listWidget->addItem("Presentation: " + QString::fromStdString(packet.decoding_result.packetVars["presentation"]));
                ui->listWidget->addItem("Unknown 1: " + QString::fromStdString(packet.decoding_result.packetVars["unknown1Hex"]));
                ui->listWidget->addItem("Unknown 2: " + QString::fromStdString(packet.decoding_result.packetVars["unknown2Hex"]));
                ui->listWidget->addItem("Unknown 3: " + QString::fromStdString(packet.decoding_result.packetVars["unknown3Hex"]));
            }
            break;
        case 0x83:
            {
                ui->listWidget->addItem("Sat: " + QString::fromStdString(packet.decoding_result.packetVars["satName"]));
                ui->listWidget->addItem("LES: " + QString::fromStdString(packet.decoding_result.packetVars["lesName"]));
                ui->listWidget->addItem("LCN: " + QString::fromStdString(packet.decoding_result.packetVars["logicalChannelNo"]));
                ui->listWidget->addItem("Message ID: " + QString::fromStdString(packet.decoding_result.packetVars["mesId"]));
                ui->listWidget->addItem("Status bits: " + QString::fromStdString(packet.decoding_result.packetVars["status_bits"]));
                ui->listWidget->addItem("Frame length: " + QString::fromStdString(packet.decoding_result.packetVars["frameLength"]));
                ui->listWidget->addItem("Duration: " + QString::fromStdString(packet.decoding_result.packetVars["duration"]));
                ui->listWidget->addItem("Downlink channel(MHz): " + QString::fromStdString(packet.decoding_result.packetVars["downlinkChannelMhz"]));
                ui->listWidget->addItem("Uplink channel(MHz): " + QString::fromStdString(packet.decoding_result.packetVars["uplinkChannelMhz"]));
                ui->listWidget->addItem("Frame offset: " + QString::fromStdString(packet.decoding_result.packetVars["frameOffset"]));
                ui->listWidget->addItem("Packet descriptor 1: " + QString::fromStdString(packet.decoding_result.packetVars["packetDescriptor1"]));
            }
            break;
        case 0x91:
            {
                //not implemented
            }
            break;
        case 0x92:
            {
                ui->listWidget->addItem("Login ack length: " + QString::fromStdString(packet.decoding_result.packetVars["loginAckLength"]));
                ui->listWidget->addItem("Downlink channel(MHz): " + QString::fromStdString(packet.decoding_result.packetVars["downlinkChannelMhz"]));
                ui->listWidget->addItem("LCN: " + QString::fromStdString(packet.decoding_result.packetVars["logicalChannelNo"]));
                ui->listWidget->addItem("Station start: " + QString::fromStdString(packet.decoding_result.packetVars["stationStartHex"]));
                if(packet.decoding_result.packetVars.find("stationCount") != packet.decoding_result.packetVars.end()) {
                    ui->listWidget->addItem("Station count: " + QString::fromStdString(packet.decoding_result.packetVars["stationCount"]));
                    ui->listWidget->addItem("Stations: \n" + QString::fromStdString(packet.decoding_result.packetVars["stations"]) + "\n");
                }
            }
            break;
        case 0x9A:
            {
                //not implemented
            }
            break;
        case 0xA0:
            {
                //not implemented
            }
            break;
        case 0xA3:
            {
                ui->listWidget->addItem("Sat: " + QString::fromStdString(packet.decoding_result.packetVars["satName"]));
                ui->listWidget->addItem("LES: " + QString::fromStdString(packet.decoding_result.packetVars["lesName"]));
                ui->listWidget->addItem("Message ID: " + QString::fromStdString(packet.decoding_result.packetVars["mesId"]));
                if(packet.decoding_result.packetVars.find("shortMessage") != packet.decoding_result.packetVars.end()) {
                    std::stringstream os;
                    for(int k = 0; k < (int)packet.decoding_result.packetVars["shortMessage"].length(); k++) {
                        char chr = packet.decoding_result.packetVars["shortMessage"][k];
                        if(chr != 0x0a && chr != 0x0d && (chr < 0x20)) {
                           os << "(0x" << std::hex << std::setfill('0') << std::setw(2) << std::right << (uint16_t)chr << std::dec << ")";;
                        } else {
                            os << chr;
                        }
                    }
                    ui->listWidget->addItem("Short message: \n" + QString::fromStdString(os.str()) + "\n");
                }
            }
            break;
        case 0xA8:
            {
                ui->listWidget->addItem("Sat: " + QString::fromStdString(packet.decoding_result.packetVars["satName"]));
                ui->listWidget->addItem("LES: " + QString::fromStdString(packet.decoding_result.packetVars["lesName"]));
                ui->listWidget->addItem("Message ID: " + QString::fromStdString(packet.decoding_result.packetVars["mesId"]));
                if(packet.decoding_result.packetVars.find("shortMessage") != packet.decoding_result.packetVars.end()) {
                    std::stringstream os;
                    for(int k = 0; k < (int)packet.decoding_result.packetVars["shortMessage"].length(); k++) {
                        char chr = packet.decoding_result.packetVars["shortMessage"][k];
                        if(chr != 0x0a && chr != 0x0d && (chr < 0x20)) {
                           os << "(0x" << std::hex << std::setfill('0') << std::setw(2) << std::right << (uint16_t)chr << std::dec << ")";
                        } else {
                            os << chr;
                        }
                    }
                ui->listWidget->addItem("Short message: \n" + QString::fromStdString(os.str()) + "\n");
                }
            }
            break;
        case 0xAA:
            {
                ui->listWidget->addItem("Sat: " + QString::fromStdString(packet.decoding_result.packetVars["satName"]));
                ui->listWidget->addItem("LES: " + QString::fromStdString(packet.decoding_result.packetVars["lesName"]));
                ui->listWidget->addItem("LCN: " + QString::fromStdString(packet.decoding_result.packetVars["logicalChannelNo"]));
                ui->listWidget->addItem("Packet NO: " + QString::fromStdString(packet.decoding_result.packetVars["packetNo"]));
                bool isBinary = packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_BINARY;
                std::stringstream os;

                ui->listWidget->addItem("Presentation: " + QString((packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_IA5) ? "IA5(text)" : ((packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_ITA2) ? "ITA2(text)" : "Binary")));
                if(packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_IA5) {
                    for(int i = 0; i < (int)packet.decoding_result.payload.data8Bit.size(); i++) {
                        char chr = packet.decoding_result.payload.data8Bit[i] & 0x7F;
                        if(chr != 0x0a && chr != 0x0d && (chr < 0x20)) {
                            os << "(0x" << std::hex << std::setfill('0') << std::setw(2) << std::right << (uint16_t)chr << std::dec << ")";
                        } else {
                            os << chr;
                        }
                    }
                } else if(packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_ITA2) {

                } else {
                    for(int i = 0; i < (int)packet.decoding_result.payload.data8Bit.size(); i++) {
                        uint8_t data = packet.decoding_result.payload.data8Bit[i];
                        os << std::hex << std::setfill('0') << std::setw(2) << std::right << (uint16_t)data << std::dec << " ";
                    }
                }

                ui->listWidget->addItem("Message: \n" + QString::fromStdString(os.str()) + "\n");
            }
            break;
        case 0xAB:
            {
                ui->listWidget->addItem("LES list length: " + QString::fromStdString(packet.decoding_result.packetVars["lesListLength"]));
                ui->listWidget->addItem("Station start: " + QString::fromStdString(packet.decoding_result.packetVars["stationStartHex"]));
                ui->listWidget->addItem("Station count: " + QString::fromStdString(packet.decoding_result.packetVars["stationCount"]));
                ui->listWidget->addItem("Stations: \n" + QString::fromStdString(packet.decoding_result.packetVars["stations"]) + "\n");
            }
            break;
        case 0xAC:
            {
                //not implemented
            }
            break;
        case 0xAD:
            {
                //not implemented
            }
            break;
        case 0xB1:
            {
                ui->listWidget->addItem("Message type: " + QString::fromStdString(packet.decoding_result.packetVars["messageType"]));
                ui->listWidget->addItem("Service code&address name: " + QString::fromStdString(packet.decoding_result.packetVars["serviceCodeAndAddressName"]));
                ui->listWidget->addItem("Continuation: " + QString::fromStdString(packet.decoding_result.packetVars["continuation"]));
                ui->listWidget->addItem("Priority: " + QString::fromStdString(packet.decoding_result.packetVars["priorityText"]));
                ui->listWidget->addItem("Repetition: " + QString::fromStdString(packet.decoding_result.packetVars["repetition"]));
                ui->listWidget->addItem("Message ID: " + QString::fromStdString(packet.decoding_result.packetVars["messageId"]));
                ui->listWidget->addItem("Packet NO: " + QString::fromStdString(packet.decoding_result.packetVars["packetNo"]));
                ui->listWidget->addItem("Is new payload: " + QString::fromStdString(packet.decoding_result.packetVars["isNewPayload"]));
                ui->listWidget->addItem("Address: " + QString::fromStdString(packet.decoding_result.packetVars["addressHex"]));
                bool isBinary = packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_BINARY;
                std::stringstream os;

                ui->listWidget->addItem("Presentation: " + QString((packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_IA5) ? "IA5(text)" : ((packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_ITA2) ? "ITA2(text)" : "Binary")));
                if(packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_IA5) {
                    for(int i = 0; i < (int)packet.decoding_result.payload.data8Bit.size(); i++) {
                        char chr = packet.decoding_result.payload.data8Bit[i] & 0x7F;
                        if(chr != 0x0a && chr != 0x0d && (chr < 0x20)) {
                            os << "(0x" << std::hex << std::setfill('0') << std::setw(2) << std::right << (uint16_t)chr << std::dec << ")";
                        } else {
                            os << chr;
                        }
                    }
                } else if(packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_ITA2) {

                } else {
                    for(int i = 0; i < (int)packet.decoding_result.payload.data8Bit.size(); i++) {
                        uint8_t data = packet.decoding_result.payload.data8Bit[i];
                        os << std::hex << std::setfill('0') << std::setw(2) << std::right << (uint16_t)data << std::dec << " ";
                    }
                }

                ui->listWidget->addItem("Message: \n" + QString::fromStdString(os.str()) + "\n");
            }
            break;
        case 0xB2:
            {
                ui->listWidget->addItem("Message type: " + QString::fromStdString(packet.decoding_result.packetVars["messageType"]));
                ui->listWidget->addItem("Service code&address name: " + QString::fromStdString(packet.decoding_result.packetVars["serviceCodeAndAddressName"]));
                ui->listWidget->addItem("Continuation: " + QString::fromStdString(packet.decoding_result.packetVars["continuation"]));
                ui->listWidget->addItem("Priority: " + QString::fromStdString(packet.decoding_result.packetVars["priorityText"]));
                ui->listWidget->addItem("Repetition: " + QString::fromStdString(packet.decoding_result.packetVars["repetition"]));
                ui->listWidget->addItem("Message ID: " + QString::fromStdString(packet.decoding_result.packetVars["messageId"]));
                ui->listWidget->addItem("Packet NO: " + QString::fromStdString(packet.decoding_result.packetVars["packetNo"]));
                ui->listWidget->addItem("Is new payload: " + QString::fromStdString(packet.decoding_result.packetVars["isNewPayload"]));
                ui->listWidget->addItem("Address: " + QString::fromStdString(packet.decoding_result.packetVars["addressHex"]));
                bool isBinary = packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_BINARY;
                std::stringstream os;

                ui->listWidget->addItem("Presentation: " + QString((packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_IA5) ? "IA5(text)" : ((packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_ITA2) ? "ITA2(text)" : "Binary")));
                if(packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_IA5) {
                    for(int i = 0; i < (int)packet.decoding_result.payload.data8Bit.size(); i++) {
                        char chr = packet.decoding_result.payload.data8Bit[i] & 0x7F;
                        if(chr != 0x0a && chr != 0x0d && (chr < 0x20)) {
                            os << "(0x" << std::hex << std::setfill('0') << std::setw(2) << std::right << (uint16_t)chr << std::dec << ")";
                        } else {
                            os << chr;
                        }
                    }
                } else if(packet.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_ITA2) {

                } else {
                    for(int i = 0; i < (int)packet.decoding_result.payload.data8Bit.size(); i++) {
                        uint8_t data = packet.decoding_result.payload.data8Bit[i];
                        os << std::hex << std::setfill('0') << std::setw(2) << std::right << (uint16_t)data << std::dec << " ";
                    }
                }

                ui->listWidget->addItem("Message: \n" + QString::fromStdString(os.str()) + "\n");
            }
            break;
        case 0xBD:
            {
                //nothing
            }
            break;
        case 0xBE:
            {
                //nothing
            }
            break;
        default:
            break;
    }
}

void QStdcdec::updateMessage(Message m) {
    ui->textEdit->clear();
    if(m.representation == PACKETDECODER_PRESENTATION_IA5) {
        std::stringstream os;
        for(int i = 0; i < (int)m.msg.length(); i++) {
            char data = m.msg[i] & 0b01111111;
            os << data;
        }
        ui->textEdit->setText(QString::fromStdString(os.str()));
    } else if(m.representation == PACKETDECODER_PRESENTATION_ITA2) {
        ui->textEdit->setText(QString::fromStdString(m.msg));
    } else {
        std::stringstream os;
        for(int i = 0; i < (int)m.msg.length(); i++) {
            uint8_t data = m.msg[i];
            os << std::hex << std::setfill('0') << std::setw(2) << std::right << (uint16_t)data << std::dec << " ";
        }
        ui->textEdit->setText(QString::fromStdString(os.str()));
    }
}

void QStdcdec::processFrame(inmarsatc::decoder::Decoder::decoder_result frame) {
    std::vector<inmarsatc::frameParser::FrameParser::frameParser_result> res = parser.parseFrame(frame);
    for(int i = 0; i < res.size(); i++) {
        if(!res[i].decoding_result.isDecodedPacket || !res[i].decoding_result.isCrc) {
            continue;
        }
        appendPacketsTableAndArray(res[i]);
        processPacketAsMessage(res[i]);
    }
}

void QStdcdec::on_pushButton_3_clicked() {
    packets.clear();
    reloadPacketsTableFromArray();
    ui->listWidget->clear();
}


void QStdcdec::on_pushButton_2_clicked() {
    loadPacketsFromFile();
}


void QStdcdec::on_checkBox_stateChanged(int arg1) {
    if(!settingsLoaded) {
        return;
    }
    if(ui->checkBox->isChecked()) {
        savePacketsToFile();
        ui->lineEdit->setEnabled(false);
    } else {
        ui->lineEdit->setEnabled(true);
    }
    saveConfigToFile();
}


void QStdcdec::on_checkBox_3_stateChanged(int arg1) {
    if(!settingsLoaded) {
        return;
    }
    if(ui->checkBox_3->isChecked()) {
        saveMessagesToFile();
        ui->lineEdit_3->setEnabled(false);
    } else {
        ui->lineEdit_3->setEnabled(true);
    }
    saveConfigToFile();
}

void QStdcdec::on_pushButton_4_clicked() {
    loadMessagesFromFile();
}


void QStdcdec::on_pushButton_5_clicked() {
    messages.clear();
    uncomplete_messages.clear();
    reloadMessagesTableFromArray();
}


void QStdcdec::on_spinBox_valueChanged(int arg1) {
    saveConfigToFile();
}


void QStdcdec::on_checkBox_2_stateChanged(int arg1) {
    saveConfigToFile();
}


void QStdcdec::on_lineEdit_textChanged(const QString &arg1) {
    saveConfigToFile();
}


void QStdcdec::on_checkBox_4_stateChanged(int arg1) {
    saveConfigToFile();
}


void QStdcdec::on_lineEdit_3_textChanged(const QString &arg1) {
    saveConfigToFile();
}

