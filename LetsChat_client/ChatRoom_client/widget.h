#ifndef WIDGET_H
#define WIDGET_H

#include "ui_widget.h"
#include "dialog.h"
#include "netinit.h"

#include <QWidget>
#include <QString>
#include <QListWidgetItem>
#include <QFile>

#define BUFFER_SIZE 4096

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QString usrName, QWidget *parent = nullptr);
    QString usrNameGetter();

    ~Widget();

private slots:
    void on_close_pb_clicked();

    void on_sendFile_pb_clicked();

    void on_send_pb_clicked();
    void on_MessageReceived(const QString &message);   

private:
    NetInit network_serve;
    QString usr_name;
    QString msg_content_send;
    Ui::Widget *ui;

    QFile file_to_send;
    QFile file_to_receive;
    qint64 bytes_sent = 0;
    qint64 bytes_received = 0;
    qint64 total_file_size = 0;
    QString received_file_name;

    //msg handle
    void handleNormalMsg(const QStringList &messageParts);
    void handleFileMsg(const QStringList &messageParts);
    void handleLogMsg(const QStringList &messageParts);

    void handleDownloadFile(const QStringList &messageParts);

    void sendFileData();
};
#endif // WIDGET_H
