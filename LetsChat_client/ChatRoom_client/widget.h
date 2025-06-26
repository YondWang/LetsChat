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
#include <QMap>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileInfo>

#define BUFFER_SIZE 8192

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
    
    void handleMessageReceived(const QString &username, const QString &message);
    void handleUserLoggedIn(int userId, const QString &userName);
    void handleUserLoggedOut(int userId);
    void handleFileBroadcastReceived(const QString &sender, const QString &filename, qint64 filesize);
    void handleFileDownloadRequested(const QString &filename, const QString &sender);
    void handleDisconnected(const QString &username);
    void handleConnectionError(const QString &error);
    
    void handleUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void handleUploadFinished();
    void handleDownloadFinished();
    void handleFileTransferError(const QString &errorMessage);

    void displayMessage(const QString& sender, const QString& message);
    void displayFileMessage(const QString& sender, const QString& filename, qint64 filesize);

private:
    MessageBroadcaster *m_broadcaster;
    FileTransfer *m_fileTransfer;
    QString m_username;
    Ui::Widget *ui;
    QThread m_thSendThread;
    
    QProgressDialog *m_uploadProgress;
    QProgressDialog *m_downloadProgress;
    
    QMap<QString, QString> m_downloadedFiles; // 文件名->本地保存路径
    QMap<int, QString> m_onlineUserName; // userId->userName
    int m_myUserId; // 当前用户id
    
    void setupConnections();
    void updateUserList();
};
#endif // WIDGET_H
