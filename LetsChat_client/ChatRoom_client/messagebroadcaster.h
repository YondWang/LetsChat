#ifndef MESSAGEBROADCASTER_H
#define MESSAGEBROADCASTER_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>

class MessageBroadcaster : public QObject
{
    Q_OBJECT
public:
    explicit MessageBroadcaster(QObject *parent = nullptr);
    ~MessageBroadcaster();

    void connectToServer(const QString &host, quint16 port);
    void sendMessage(const QString &message);
    void sendLoginBroadcast(const QString &username);
    void sendFileBroadcast(const QString &filename, qint64 filesize);
    void requestFileDownload(const QString &filename, const QString &sender);

signals:
    void messageReceived(const QString &sender, const QString &message);
    void userLoggedIn(const QString &username);
    void userLoggedOut(const QString &username);
    void fileBroadcastReceived(const QString &sender, const QString &filename, qint64 filesize);
    void fileDownloadRequested(const QString &filename, const QString &sender);
    void connectionError(const QString &error);

private slots:
    void handleReadyRead();
    void handleConnected();
    void handleDisconnected();
    void handleError(QAbstractSocket::SocketError socketError);

private:
    enum MessageType {
        YConnect,
        YMsg,
        YFile,
        YRecv
    };

    // 写入大端序的16位整数
    static void writeUint16(char*& pData, quint16 value) {
        pData[0] = (value >> 8) & 0xFF;
        pData[1] = value & 0xFF;
        pData += 2;
    }

    // 写入大端序的32位整数
    static void writeUint32(char*& pData, quint32 value) {
        pData[0] = (value >> 24) & 0xFF;
        pData[1] = (value >> 16) & 0xFF;
        pData[2] = (value >> 8) & 0xFF;
        pData[3] = value & 0xFF;
        pData += 4;
    }

    // 读取大端序的16位整数
    static quint16 readUint16(const QByteArray& data, int& pos) {
        quint16 value = ((quint8)data[pos] << 8) | (quint8)data[pos + 1];
        pos += 2;
        return value;
    }

    // 读取大端序的32位整数
    static quint32 readUint32(const QByteArray& data, int& pos) {
        quint32 value = ((quint8)data[pos] << 24) | ((quint8)data[pos + 1] << 16) |
                       ((quint8)data[pos + 2] << 8) | (quint8)data[pos + 3];
        pos += 4;
        return value;
    }

    QByteArray createMessagePacket(MessageType type, const QString &data);
    void parseMessage(const QByteArray &data);

    QTcpSocket *m_socket;
    bool m_isConnected;
    QByteArray m_buffer;
};

#endif // MESSAGEBROADCASTER_H 
