#include "messagebroadcaster.h"
#include <QDebug>
#include <QDataStream>
#include <QtEndian>  // 添加Qt的字节序转换头文件
#include <Winsock2.h>  // 添加Winsock2.h头文件
#pragma comment(lib, "ws2_32.lib")  // 链接Winsock库

// 消息头部标识
const unsigned short MESSAGE_HEADER = 0xFEFF;

MessageBroadcaster::MessageBroadcaster(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_isConnected(false)
    , m_currentUserId(0)
    , m_username("")
{
    connect(m_socket, &QTcpSocket::readyRead, this, &MessageBroadcaster::handleReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &MessageBroadcaster::handleConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &MessageBroadcaster::handleDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &MessageBroadcaster::handleError);
}

MessageBroadcaster::~MessageBroadcaster()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

void MessageBroadcaster::connectToServer(const QString &host, quint16 port)
{
    m_socket->connectToHost(host, port);
}

// 写入大端序的16位整数
void writeUint16(char*& pData, quint16 value) {
    pData[0] = (value >> 8) & 0xFF;
    pData[1] = value & 0xFF;
    pData += 2;
}

// 写入大端序的32位整数
void writeUint32(char*& pData, quint32 value) {
    pData[0] = (value >> 24) & 0xFF;
    pData[1] = (value >> 16) & 0xFF;
    pData[2] = (value >> 8) & 0xFF;
    pData[3] = value & 0xFF;
    pData += 4;
}

QByteArray MessageBroadcaster::createMessagePacket(MessageType type, const QString &data)
{
    // 确保使用 UTF-8 编码
    QByteArray dataBytes = data.toUtf8();
    quint32 length = dataBytes.size() + 4; // 4 = 命令类型(2字节) + 用户ID(2字节)

    // 预分配空间
    QByteArray packet;
    packet.resize(2 + 4 + 2 + 2 + dataBytes.size() + 2); // 头部(2) + 长度(4) + 命令(2) + 用户ID(2) + 数据 + 校验和(2)
    char* pData = packet.data();

    // 写入消息头（使用网络字节序）
    quint16 header = htons(MESSAGE_HEADER);
    memcpy(pData, &header, 2);
    pData += 2;

    // 写入长度（使用网络字节序）
    quint32 netLength = htonl(length);
    memcpy(pData, &netLength, 4);
    pData += 4;

    // 写入命令（使用网络字节序）
    quint16 netType = htons(type);
    memcpy(pData, &netType, 2);
    pData += 2;

    // 写入用户ID（使用网络字节序）
    quint16 userId = htons(m_currentUserId);  // 使用当前用户的ID
    memcpy(pData, &userId, 2);
    pData += 2;

    // 写入数据
    memcpy(pData, dataBytes.constData(), dataBytes.size());
    pData += dataBytes.size();

    // 计算并写入校验和（使用网络字节序）
    quint16 sum = 0;
    for (int i = 0; i < dataBytes.size(); i++) {
        sum += (quint8)dataBytes[i];
    }
    quint16 netSum = htons(sum);
    memcpy(pData, &netSum, 2);

    return packet;
}

// 读取大端序的16位整数
quint16 readUint16(const QByteArray& data, int& pos) {
    quint16 value = ((quint8)data[pos] << 8) | (quint8)data[pos + 1];
    pos += 2;
    return value;
}

// 读取大端序的32位整数
quint32 readUint32(const QByteArray& data, int& pos) {
    quint32 value = ((quint8)data[pos] << 24) | ((quint8)data[pos + 1] << 16) |
                   ((quint8)data[pos + 2] << 8) | (quint8)data[pos + 3];
    pos += 4;
    return value;
}

void MessageBroadcaster::sendMessage(const QString &message)
{
    QByteArray packet = createMessagePacket(YMsg, message);
    qDebug() << "Sending message:" << message;
    m_socket->write(packet);
}

void MessageBroadcaster::sendLoginBroadcast(const QString &username)
{
    m_username = username;  // 保存用户名
    QByteArray packet = createMessagePacket(YConnect, username);
    qDebug() << "Sending login broadcast:" << username;
    m_socket->write(packet);
}

void MessageBroadcaster::sendFileBroadcast(const QString &filename, qint64 filesize)
{
    QString data = QString("%1 %2").arg(filename).arg(filesize);
    QByteArray packet = createMessagePacket(YFile, data);
    qDebug() << "Sending file broadcast:" << data;
    m_socket->write(packet);
}

void MessageBroadcaster::requestFileDownload(const QString &filename, const QString &sender)
{
    QString data = QString("%1 %2").arg(filename).arg(sender);
    QByteArray packet = createMessagePacket(YRecv, data);
    qDebug() << "Requesting file download:" << data;
    m_socket->write(packet);
}

void MessageBroadcaster::handleReadyRead()
{
    QByteArray data = m_socket->readAll();
    qDebug() << "Received raw data:" << data.toHex();
    m_buffer.append(data);

    while (m_buffer.size() >= 2) {
        // 查找消息头部
        int headerPos = -1;
        for (int i = 0; i <= m_buffer.size() - 2; i++) {
            quint16 header;
            memcpy(&header, m_buffer.constData() + i, 2);
            header = ntohs(header);
            if (header == MESSAGE_HEADER) {
                headerPos = i;
                break;
            }
        }

        if (headerPos == -1) {
            if (m_buffer.size() > 1) {
                m_buffer = m_buffer.right(1);
            }
            return;
        }

        if (headerPos > 0) {
            m_buffer.remove(0, headerPos);
        }

        if (m_buffer.size() < 10) return;  // 至少需要头部(2) + 长度(4) + 命令(2) + 用户ID(2)

        int pos = 0;
        quint16 header;
        memcpy(&header, m_buffer.constData() + pos, 2);
        header = ntohs(header);
        pos += 2;

        quint32 length;
        memcpy(&length, m_buffer.constData() + pos, 4);
        length = ntohl(length);
        pos += 4;

        if (m_buffer.size() < length + 8) return;  // 等待完整消息

        QByteArray message = m_buffer.left(length + 8);
        m_buffer.remove(0, length + 8);

        parseMessage(message);
    }
}

void MessageBroadcaster::parseMessage(const QByteArray &data)
{
    if (data.size() < 10) return;  // 至少需要头部(2) + 长度(4) + 命令(2) + 用户ID(2)

    int pos = 0;
    quint16 header;
    memcpy(&header, data.constData() + pos, 2);
    header = ntohs(header);
    pos += 2;

    if (header != MESSAGE_HEADER) {
        qDebug() << "Invalid message header:" << header;
        return;
    }

    quint32 length;
    memcpy(&length, data.constData() + pos, 4);
    length = ntohl(length);
    pos += 4;

    if (length > data.size() - 8) {
        qDebug() << "Invalid message length:" << length;
        return;
    }

    quint16 cmd;
    memcpy(&cmd, data.constData() + pos, 2);
    cmd = ntohs(cmd);
    pos += 2;

    quint16 userId;
    memcpy(&userId, data.constData() + pos, 2);
    userId = ntohs(userId);
    pos += 2;

    int userIdInt = userId;
    QByteArray messageData = data.mid(pos, length - 4);

    // 确保使用 UTF-8 解码
    QString message = QString::fromUtf8(messageData);
    qDebug() << "Parsed message:" << message;

    int tempPos = pos + messageData.size();
    quint16 sum;
    memcpy(&sum, data.constData() + tempPos, 2);
    sum = ntohs(sum);

    quint16 calculatedSum = 0;
    for (int i = 0; i < messageData.size(); i++) {
        calculatedSum += (quint8)messageData[i];
    }
    if (calculatedSum != sum) {
        qDebug() << "Checksum mismatch:" << sum << "!=" << calculatedSum;
        return;
    }

    switch (cmd) {
        case YConnect:
            //m_userIdToName[userIdInt] = message;
            m_userIdToName[userIdInt] = message;
            emit userLoggedIn(message);
            break;
        case YMsg:
            qDebug() << "msg from:" + m_userIdToName[userIdInt] + " message:" + message;
            emit messageReceived(m_userIdToName[userIdInt], message);
            break;

        case YDisCon:
            emit userLoggedOut(message);
            m_userIdToName.erase(userIdInt);
            break;
        case YFile:
            {
                QStringList parts = message.split(" ");
                if (parts.size() >= 2) {
                    bool ok;
                    qint64 fileSize = parts[1].toLongLong(&ok);
                    if (ok) {
                        emit fileBroadcastReceived(m_userIdToName[userIdInt], parts[0], fileSize);
                    }
                }
            }
            break;
        case YRecv:
            {
                QStringList parts = message.split(" ");
                if (parts.size() >= 2) {
                    emit fileDownloadRequested(parts[0], parts[1]);
                }
            }
            break;
        default:
            qDebug() << "Unknown message type:" << cmd;
    }
}

void MessageBroadcaster::handleConnected()
{
    m_isConnected = true;
    //emit connectionError("");
}

void MessageBroadcaster::handleDisconnected()
{
    m_isConnected = false;
    emit connectionError("与服务器断开连接");
}

void MessageBroadcaster::handleError(QAbstractSocket::SocketError socketError)
{
    m_isConnected = false;
    emit connectionError(m_socket->errorString());
}
