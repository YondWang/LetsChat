#include "widget.h"
#include <QDebug>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QWidget>
#include <QtWidgets>
#include <QSocketNotifier>
#define PACKAGE_SIZE 4096

Widget::Widget(QString usrName, QWidget *parent)
    : QWidget(parent), network_serve(usrName), usr_name(usrName)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    //qDebug() << usrName;
    ui->userName_tl->setText(usrName);

    int net_ret = network_serve.initialize("127.0.0.1");
    if(net_ret == -1){
        QMessageBox::critical(this, "error", "failed to connect to server");
        exit(0);
    }

    connect(&network_serve, &NetInit::messageToShow, this, &Widget::on_MessageReceived);
}

QString Widget::usrNameGetter()
{
    return usr_name;
}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_close_pb_clicked()
{
    network_serve.sendMessage("ESC ");
    exit(0);
}

void Widget::on_sendFile_pb_clicked()
{
    QString file_name = QFileDialog::getOpenFileName(this, u8"请选择要发送的文件", "D:\\");
    if(!file_name.isEmpty()){
        qDebug() << "Select file:" << file_name;
    }

    file_to_send.setFileName(file_name);
    if(!file_to_send.open(QIODevice::ReadOnly)){
        QMessageBox::warning(this, "error", "unable to open file");
        return;
    }

    QString fileInfo = QString("FILE %1 %2")
            .arg(file_to_send.fileName().section("/", -1))       //send filename only
            .arg(file_to_send.size());
    network_serve.sendMessage(fileInfo);

    // Create custom widget
    QWidget *itemWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(itemWidget);
    QLabel *fileNameLabel = new QLabel(file_to_send.fileName().section("/", -1) + " : upload by your self!");
    //QPushButton *downloadButton = new QPushButton(u8"下载");
    QPushButton *openButton = new QPushButton(u8"打开");

    // Add widgets to layout
    layout->addWidget(fileNameLabel);
    //layout->addWidget(downloadButton);
    layout->addWidget(openButton);
    layout->setSpacing(5);
    layout->setContentsMargins(5, 5, 5, 5);

    //create QListWidgetItem
    QListWidgetItem *item = new QListWidgetItem(ui->fileList_lw);
    item->setSizeHint(itemWidget->sizeHint());
    ui->fileList_lw->addItem(item);
    ui->fileList_lw->setItemWidget(item, itemWidget);

    //connect buttons
    //    connect(downloadButton, &QPushButton::clicked, this, [=]() {
    //       QMessageBox::information(this, "Download", "Downloading file: " + file_to_send.fileName());
    //       requestFileDownload(file_to_send.fileName());
    //    });

    connect(openButton, &QPushButton::clicked, this, [=]() {
        //QDesktopServices::openUrl(QUrl::fromLocalFile(file_to_send.fileName()));
        //TODO: if is not in local, qmessagebox. else open it
        QDesktopServices::openUrl(QUrl::fromLocalFile(file_to_send.fileName()));
    });

    bytes_sent = 0;
}

void Widget::on_send_pb_clicked()
{
    //get the user enter and send
    msg_content_send = "MSG " + ui->send_te->toPlainText();
    qDebug() << msg_content_send;
    network_serve.sendMessage(msg_content_send);

    QDateTime date_time = QDateTime::currentDateTime();
    QString time_str = date_time.toString("yyyy-MM-dd hh:mm:ss");

    QString msg_display = time_str + u8" 我:\n" + ui->send_te->toPlainText();
    QListWidgetItem *item = new QListWidgetItem(msg_display);

    //add send msg to chatwindow_lw right
    item->setTextAlignment(Qt::AlignRight);         //right side

    //    QPixmap pixmap(10, 10);
    //    pixmap.fill(Qt::transparent);
    //    QPainter painter(&pixmap);
    //    painter.setPen(QPen(Qt::black, 2));
    //    painter.drawLine(0, 0, pixmap.width(), 0);
    //    item->setBackground(QBrush(pixmap));

    ui->chatWindow_lw->addItem(item);               //add msg to list
    ui->chatWindow_lw->scrollToBottom();            //scroll to bottom

    ui->send_te->setText(NULL);
}

void Widget::on_MessageReceived(const QString &message)
{
    qDebug() << "recived:" << message;

    QStringList messageParts = message.split(" ");
    if(messageParts.isEmpty()){
        QMessageBox::information(this, "notice!", "empty or invalid message format~!");
        return;
    }

    QString type = message.split(" ").at(0);

    if(type == "MSG"){
        handleNormalMsg(messageParts);
    }else if(type == "ACP"){
        sendFileData();
    }else if(type == "ACPF"){
        handleDownloadFile(messageParts);
    }else if(type == "FILE"){
        handleFileMsg(messageParts);
    }else if(type == "LOG"){
        handleLogMsg(messageParts);
    }else {
        qDebug() << "recive type:" << type;
        QMessageBox::information(this, "notice!", "unknow message type!!");
    }


    /*QString dis_msg = message.split(" ").at(1);
        QString dis_usr = message.split(" ").at(2);


        QDateTime date_time = QDateTime::currentDateTime();
        QString time_str = date_time.toString("yyyy-MM-dd hh:mm:ss");
        QString msg_display = time_str + " " + dis_usr + ":\n" + dis_msg;

        QListWidgetItem *item = new QListWidgetItem(msg_display);

        if("LOG" != message.split(" ").at(0)){
            item->setTextAlignment(Qt::AlignLeft);       //left side
            ui->chatWindow_lw->addItem(item);            //add msg to list
            ui->chatWindow_lw->scrollToBottom();         //scroll to bottom
        }else{
            item->setTextAlignment(Qt::AlignCenter);     //put notice in center
            QFont font;
            font.setFamily("宋体");
            font.setPointSize(9);
            font.setStyle(QFont::StyleItalic);
            item->setFont(font);
            ui->chatWindow_lw->addItem(item);            //add msg to list
            ui->chatWindow_lw->scrollToBottom();         //scroll to bottom
        }*/
}

void Widget::handleNormalMsg(const QStringList &messageParts)
{
    if(messageParts.size() < 3){
        qWarning() << "invalid chat message format";
        return ;
    }

    QString sender = messageParts.at(2);
    QString content = messageParts.mid(1, messageParts.size() -2).join(" ");

    QDateTime date_time = QDateTime::currentDateTime();
    QString time_str = date_time.toString("yyyy-MM-dd hh:mm:ss");

    QString msg_display = time_str + " " + sender + ":\n" + content;
    QListWidgetItem *item = new QListWidgetItem(msg_display);
    item->setTextAlignment(Qt::AlignLeft);

    ui->chatWindow_lw->addItem(item);
    ui->chatWindow_lw->scrollToBottom();
}

void Widget::handleFileMsg(const QStringList &messageParts)
{
    if(messageParts.size() < 3){
        qWarning() << "invalid chat message format";
        return ;
    }

    QString file_name = messageParts.at(1);
    QString file_size = messageParts.at(2);
    QString from_usr = messageParts.at(3);

    // Create custom widget
    QWidget *itemWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(itemWidget);
    QLabel *fileNameLabel = new QLabel(file_name + " : upload by " + from_usr + " size:" + file_size);
    QPushButton *downloadButton = new QPushButton(u8"下载");
    QPushButton *openButton = new QPushButton(u8"打开");

    // Add widgets to layout
    layout->addWidget(fileNameLabel);
    layout->addWidget(downloadButton);
    layout->addWidget(openButton);
    layout->setSpacing(5);
    layout->setContentsMargins(5, 5, 5, 5);

    // Create QListWidgetItem
    QListWidgetItem *item = new QListWidgetItem(ui->fileList_lw);
    item->setSizeHint(itemWidget->sizeHint());
    ui->fileList_lw->addItem(item);
    ui->fileList_lw->setItemWidget(item, itemWidget);

    //connect buttons
    connect(downloadButton, &QPushButton::clicked, this, [=]() {
        QMessageBox::information(this, "Download", "Downloading file: " + file_to_send.fileName());

        QString rqst_msg = QString("RQST %1").arg(file_name);
        network_serve.sendMessage(rqst_msg);
    });

    connect(openButton, &QPushButton::clicked, this, [=]() {
        //QDesktopServices::openUrl(QUrl::fromLocalFile(file_to_send.fileName()));
        //TODO: if is not in local, qmessagebox. else open it
        QString local_path = QDir::currentPath() + "/appRecFile/" + file_name;
        if(QFile::exists(local_path)){
            QDesktopServices::openUrl(QUrl::fromLocalFile(local_path));
        }else{
            QMessageBox::warning(this, "error", "file not found locally, plz download first!");
        }
    });
}

void Widget::handleLogMsg(const QStringList &messageParts)
{
    if(messageParts.size() < 2){
        qWarning() << "invalid log message format";
        return ;
    }

    QString log_content = messageParts.mid(1).join(" ");

    QDateTime date_time = QDateTime::currentDateTime();
    QString time_str = date_time.toString("yyyy-MM-dd hh:mm:ss");

    QString log_display = time_str + " [System Log]:\n" + log_content;
    QListWidgetItem *item = new QListWidgetItem(log_display);
    item->setTextAlignment(Qt::AlignCenter);

    QFont font;
    font.setFamily("宋体");
    font.setPointSize(9);
    font.setStyle(QFont::StyleItalic);
    item->setFont(font);

    ui->chatWindow_lw->addItem(item);
    ui->chatWindow_lw->scrollToBottom();
}

void Widget::handleDownloadFile(const QStringList &messageParts)
{
    if (messageParts.size() < 3) {
            qWarning() << "Invalid file message format";
            return;
        }

        QString file_name = messageParts.at(1);
        qint64 file_size = messageParts.at(2).toLongLong();

        // 准备保存目录
        QString save_path = QDir::currentPath() + "/appRecFile";
        QDir dir(save_path);
        if (!dir.exists() && !dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << save_path;
            return;
        }

        // 打开文件
        file_to_receive.setFileName(save_path + "/" + file_name);
        if (!file_to_receive.open(QIODevice::WriteOnly)) {
            qWarning() << "Cannot open file for writing:" << file_name;
            return;
        }

        // 初始化接收
        bytes_received = 0;
        total_file_size = file_size;

        // 通知服务器开始传输
        network_serve.sendMessage("BGN");

        // 接收循环
        bool success = true;
        while (bytes_received < total_file_size) {
            QByteArray buffer = network_serve.receiveMessage();
            if (buffer.isEmpty()) {
                QThread::msleep(10);  // 短暂休眠避免CPU占用过高
                continue;
            }

            qint64 written = file_to_receive.write(buffer);
            if (written == -1) {
                success = false;
                qWarning() << "Write error:" << file_to_receive.errorString();
                break;
            }

            bytes_received += written;
        }

        // 清理和检查结果
        file_to_receive.close();

        if (!success || bytes_received < total_file_size) {
            file_to_receive.remove();
            qWarning() << "Download failed - received" << bytes_received << "of" << total_file_size;
        }
}

void Widget::sendFileData()
{
    if(!file_to_send.isOpen()) return;

    QByteArray buffer = file_to_send.read(BUFFER_SIZE);
    while(!buffer.isEmpty()){
        network_serve.sendMessage(buffer);
        bytes_sent += buffer.size();
        buffer = file_to_send.read(BUFFER_SIZE);
    }

    if(bytes_sent >= file_to_send.size()){
        file_to_send.close();
        QMessageBox::information(this, "success", "file finish send");
    }
}









