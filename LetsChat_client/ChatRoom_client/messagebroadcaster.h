#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QMap>
#include <QString>

class MessageBroadcaster : public QObject
{
public:
    enum MessageType {
        YConnect = 0,
        YMsg = 1,
        YFile = 2,
        YRecv = 3,
        YDisCon = 4,
        YFileStart = 5,
        YFileData = 6,
        YFileEnd = 7,
        YFileAck = 8,
        YRetrans = 9,
        YUserList = 10
    };
    Q_OBJECT
public:
    explicit MessageBroadcaster(QObject *parent = nullptr);
    ~MessageBroadcaster();

    void connectToServer(const QString &host, quint16 port);
    void sendDataByPackCut(const QString &message, MessageType messageType = YMsg);
    void sendFileBroadcast(const QString &filename, qint64 filesize);
    void requestFileDownload(const QString &filename, const QString &sender);

signals:
    void messageReceived(const QString &username, const QString &message);
    void userLoggedIn(const QString &username);
    void userLoggedOut(const QString &username);
    void fileBroadcastReceived(const QString &sender, const QString &filename, qint64 filesize);
    void fileDownloadRequested(const QString &filename, const QString &sender);
    void disconnection(const QString &username);
    void connectionError(const QString &error);
private slots:
    void handleReadyRead();
    void handleConnected();
    void handleDisconnected();
    void handleError(QAbstractSocket::SocketError socketError);

private:
    static const quint16 MESSAGE_HEADER = 0xFEFF;
    QList<QByteArray> createMessagePacket(MessageType type, const QString &data);
    void parseMessage(const QByteArray &data);
    static const int MAX_PACKET_SIZE = 2048;
    QTcpSocket *m_socket;
    QByteArray m_buffer;
    bool m_isConnected;
    int m_currentUserId;
    QString m_username;
}; 
