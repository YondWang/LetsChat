#ifndef WIDGET_H
#define WIDGET_H

#include "ui_widget.h"
#include "dialog.h"
#include "messagebroadcaster.h"
#include "filetransfer.h"

#include <QWidget>
#include <QString>
#include <QListWidgetItem>
#include <QFile>
#include <QProgressDialog>
#include <QTime>

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
    
    void handleMessageReceived(const QString &sender, const QString &message);
    void handleUserLoggedIn(const QString &username);
    void handleUserLoggedOut(const QString &username);
    void handleFileBroadcastReceived(const QString &sender, const QString &filename, qint64 filesize);
    void handleFileDownloadRequested(const QString &filename, const QString &sender);
    void handleConnectionError(const QString &error);
    
    void handleUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void handleUploadFinished();
    void handleDownloadFinished();
    void handleFileTransferError(const QString &errorMessage);

private:
    MessageBroadcaster *m_broadcaster;
    FileTransfer *m_fileTransfer;
    QString m_username;
    Ui::Widget *ui;
    
    QProgressDialog *m_uploadProgress;
    QProgressDialog *m_downloadProgress;
    
    void setupConnections();
    void updateUserList();
    void displayMessage(const QString &sender, const QString &message);
    void displayFileMessage(const QString &sender, const QString &filename, qint64 filesize);
};
#endif // WIDGET_H
