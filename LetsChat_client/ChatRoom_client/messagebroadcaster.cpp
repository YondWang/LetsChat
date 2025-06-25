#include "messagebroadcaster.h"
#include <QDebug>
#include <QDataStream>
#include <QNetworkProxy>
#include <QtEndian>  // 添加Qt的字节序转换头文件
#include <Winsock2.h>  // 添加Winsock2.h头文件
#pragma comment(lib, "ws2_32.lib")  // 链接Winsock库
#include <QFile>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrent>
#include <QEventLoop>

// 消息头部标识
const unsigned short MESSAGE_HEADER = 0xFEFF;

MessageBroadcaster::MessageBroadcaster(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_isConnected(false)
    , m_currentUserId(0)
    , m_username("")
    , m_fileSendTotal(0)
    , m_fileSendCurrent(0)
    , m_lastChunkSize(0)
    , m_lastSendFileName("")
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
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
    m_socket->setProxy(QNetworkProxy::NoProxy);
    m_socket->connectToHost(host, port);
    if (!m_socket->waitForConnected(3000)) {
        emit connectionError(QString("无法连接到服务器 %1:%2，错误信息：%3").arg(host).arg(port).arg(m_socket->errorString()));
    }
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

QList<QByteArray> MessageBroadcaster::createMessagePacket(MessageType type, const QByteArray &data)
{
    QList<QByteArray> packets;
    int totalSize = data.size();
    int offset = 0;
    const int DATA_MAX = 8192; // 只分数据部分
    while (offset < totalSize) {
        int currentSize = qMin(DATA_MAX, totalSize - offset);
        QByteArray currentData = data.mid(offset, currentSize);
        qint32 length = currentData.size() + 4;  // 数据长度 + 命令(2) + 用户ID(2)
        QByteArray packet;
        packet.resize(2 + 4 + 2 + 2 + currentData.size() + 2); // 头部(2) + 长度(4) + 命令(2) + 用户ID(2) + 数据 + 校验和(2)
        char* pData = packet.data();
        quint16 header = htons(MESSAGE_HEADER);
        memcpy(pData, &header, 2);
        pData += 2;
        quint32 netLength = htonl(length);
        memcpy(pData, &netLength, 4);
        pData += 4;
        quint16 netType = htons(type);
        memcpy(pData, &netType, 2);
        pData += 2;
        quint16 userId = htons(m_currentUserId);
        memcpy(pData, &userId, 2);
        pData += 2;
        memcpy(pData, currentData.constData(), currentData.size());
        pData += currentData.size();
        quint16 sum = 0;
        for (int j = 0; j < currentData.size(); j++) {
            sum += (quint8)currentData[j];
        }
        quint16 netSum = htons(sum);
        memcpy(pData, &netSum, 2);
        packets.append(packet);
        offset += currentSize;
    }
    return packets;
}

QList<QByteArray> MessageBroadcaster::createMessagePacket(MessageType type, const QString &data)
{
    return createMessagePacket(type, data.toUtf8());
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

void MessageBroadcaster::sendDataByPackCut(const QString &message, MessageType messageType)
{
    QList<QByteArray> packets = createMessagePacket(messageType, message.toUtf8());
    qDebug() << "Sending message:" << message;
    for(int i = 0; i < packets.size(); i++){
        m_socket->write(packets[i]);
        m_socket->waitForBytesWritten(1000);
        qDebug() << packets[i];
    }
}

void MessageBroadcaster::sendFileBroadcast(const QString &filename, qint64 filesize)
{
    QString data = QString("%1 %2").arg(filename).arg(filesize);
    QList<QByteArray> packets = createMessagePacket(YFile, data.toUtf8());
    qDebug() << "Sending file broadcast:" << data;
    for (const QByteArray& packet : packets) {
        m_socket->write(packet);
    }
}

void MessageBroadcaster::requestFileDownload(const QString &filename, const QString &sender)
{
    QString data = QString("%1 %2").arg(filename).arg(sender);
    QList<QByteArray> packets = createMessagePacket(YRecv, data.toUtf8());
    qDebug() << "Requesting file download:" << data;
    for (const QByteArray& packet : packets) {
        m_socket->write(packet);
    }
}

void MessageBroadcaster::handleReadyRead()
{
    QByteArray data = m_socket->readAll();
    parseMessage(data);
}

void MessageBroadcaster::parseMessage(const QByteArray &data)
{
    if (data.size() < 12) return;  // 至少需要头部(2) + 长度(4) + 命令(2) + 用户ID(2)

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

    if (data.size() < length + 8) return;  // 等待完整消息

    quint16 cmd;
    memcpy(&cmd, data.constData() + pos, 2);
    cmd = ntohs(cmd);
    pos += 2;

    quint16 userId;
    memcpy(&userId, data.constData() + pos, 2);
    userId = ntohs(userId);
    pos += 2;

    // 数据内容长度 = length - 4（命令2+用户ID2）
    QByteArray messageData = data.mid(pos, length - 4);

    // 计算校验和
    quint16 sum;
    memcpy(&sum, data.constData() + pos + messageData.size(), 2);
    sum = ntohs(sum);

    quint16 calculatedSum = 0;
    for (int i = 0; i < messageData.size(); i++) {
        calculatedSum += (quint8)messageData[i];
    }

    if (calculatedSum != sum) {
        qDebug() << "Checksum mismatch:" << sum << "!=" << calculatedSum;
        return;
    }

    // 处理特殊命令
    if (cmd == YRetrans) {
        return;
    }
    qDebug() << "begain process the msg";
    // 确保使用 UTF-8 解码
    QString message = QString::fromUtf8(messageData);
    
    switch (static_cast<MessageType>(cmd)) {
        case YConnect:
            emit userLoggedIn(userId, message);
            break;
        case YMsg:
            emit messageReceived(QString::number(userId), message);
            break;
        case YFile:
            {
                QStringList parts = message.split(' ');
                if (parts.size() >= 2) {
                    QString filename = parts[0];
                    qint64 filesize = parts[1].toLongLong();
                    emit fileBroadcastReceived(QString::number(userId), filename, filesize);
                }
            }
            break;
        case YUserList:
        {
            // message: userId|userName;userId|userName;...
            QStringList users = message.split(';', QString::SkipEmptyParts);
            for (const QString& user : users) {
                QStringList parts = user.split('|');
                if (parts.size() == 2) {
                    int uid = parts[0].toInt();
                    QString uname = parts[1];
                    emit userLoggedIn(uid, uname);
                }
            }
            break;
        }
        case YDisCon:
            emit userLoggedOut(userId);
            break;
        case YRecv:
            {
                QStringList parts = message.split(" ");
                if (parts.size() >= 2) {
                    QString filename = parts[0];
                    QString sender = parts[1];
                    emit fileDownloadRequested(filename, sender);
                }
            }
            break;
        case YFileAck:
        {
            QString ackType = QString::fromUtf8(messageData);
            emit fileAckReceived(ackType);
            break;
        }
        default:
            qDebug() << "Unknown message type:" << cmd;
            break;
    }
}

void MessageBroadcaster::handleConnected()
{
    m_isConnected = true;
    qDebug() << "Connected to server";
    emit connected();
}

void MessageBroadcaster::handleDisconnected()
{
    m_isConnected = false;
    qDebug() << "Disconnected from server";
    emit disconnection(m_username);
}

void MessageBroadcaster::handleError(QAbstractSocket::SocketError socketError)
{
    QString errorMessage = m_socket->errorString();
    qDebug() << "Socket error:" << errorMessage;
    emit connectionError(errorMessage);
}

void MessageBroadcaster::sendFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件:" << filePath;
        return;
    }
    QString fileName = QFileInfo(filePath).fileName();
    m_lastSendFileName = fileName;
    qint64 totalSize = file.size();
    m_fileSendTotal = totalSize;
    m_fileSendCurrent = 0;
    m_lastChunkSize = 0;
    m_fileSendQueue.clear();
    m_fileSendDataLens.clear();
    m_fileSendIndex = 0;
    m_waitingFileAck = false;
    m_expectedAckType.clear();

    // 1. 发送YFileStart（文本消息）
    sendDataByPackCut(fileName + "|" + QString::number(totalSize), YFileStart);
    qDebug() << "[File] Send YFileStart:" << fileName << totalSize;
    m_expectedAckType = "START";
    m_waitingFileAck = true;

    // 连接ACK信号到槽
    connect(this, &MessageBroadcaster::fileAckReceived, this, &MessageBroadcaster::handleFileAck, Qt::UniqueConnection);

    // 2. 预分包所有YFileData，并记录每个分包的数据长度
    while (!file.atEnd()) {
        QByteArray chunk = file.read(8192);
        QList<QByteArray> dataPackets = createMessagePacket(YFileData, chunk);
        for (const QByteArray& packet : dataPackets) {
            m_fileSendQueue.enqueue(packet);
            m_fileSendDataLens.enqueue(chunk.size()); // 记录数据长度
        }
    }
    file.close();
}

void MessageBroadcaster::sendNextFileChunk()
{
    if (m_fileSendQueue.isEmpty()) {
        // 发送YFileEnd，内容为文件名
        QString fileName = m_lastSendFileName;
        sendDataByPackCut(fileName, YFileEnd);
        qDebug() << "[File] Send YFileEnd:" << fileName;
        m_expectedAckType = "END";
        m_waitingFileAck = true;
        return;
    }
    QByteArray packet = m_fileSendQueue.dequeue();
    m_lastChunkSize = m_fileSendDataLens.dequeue(); // 只记录数据部分长度
    m_socket->write(packet);
    m_socket->waitForBytesWritten(1000);
    qDebug() << QString("[File] Send YFileData chunk %1, size %2").arg(m_fileSendIndex++).arg(m_lastChunkSize);
    m_expectedAckType = "DATA";
    m_waitingFileAck = true;
}

void MessageBroadcaster::handleFileAck(const QString& ackType)
{
    if (!m_waitingFileAck) return;
    if (ackType == m_expectedAckType) {
        m_waitingFileAck = false;
        if (ackType == "START") {
            // 开始分包发送
            sendNextFileChunk();
        } else if (ackType == "DATA") {
            m_fileSendCurrent += m_lastChunkSize;
            qDebug() << QString("transfer byte info: %1/%2").arg(m_fileSendCurrent).arg(m_fileSendTotal);
            sendNextFileChunk();
        } else if (ackType == "END") {
            qDebug() << "[File] complete send file";
            disconnect(this, &MessageBroadcaster::fileAckReceived, this, &MessageBroadcaster::handleFileAck);
        }
    }
}
