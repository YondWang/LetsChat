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

void MessageBroadcaster::handleRetransmissionRequest(const QByteArray &data) {
    QString request = QString::fromUtf8(data);
    QStringList parts = request.split(",");
    if (parts.size() != 2) {
        qDebug() << "Invalid retransmission request format";
        return;
    }

    bool ok1, ok2;
    quint16 startSeq = parts[0].toUShort(&ok1);
    quint16 endSeq = parts[1].toUShort(&ok2);
    
    if (!ok1 || !ok2) {
        qDebug() << "Invalid sequence numbers in retransmission request";
        return;
    }

    retransmitMessages(startSeq, endSeq);
}

void MessageBroadcaster::retransmitMessages(quint16 startSeq, quint16 endSeq) {
    for (quint16 seq = startSeq; seq <= endSeq; ++seq) {
        if (m_messageCache.contains(seq)) {
            const CachedMessage &msg = m_messageCache[seq];
            m_socket->write(msg.rawData);
            m_socket->flush();
            qDebug() << "Retransmitted message with sequence" << seq;
        }
    }
}

QList<QByteArray> MessageBroadcaster::createMessagePacket(MessageType type, const QString &data)
{
    // 确保使用 UTF-8 编码
    QByteArray dataBytes = data.toUtf8();

    QList<QByteArray> packets;
    int totalPackets = (dataBytes.size() + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;

    for(int i = 0; i < totalPackets; i++) {
        int currentSize = qMin(MAX_PACKET_SIZE, dataBytes.size() - i * MAX_PACKET_SIZE);
        QByteArray currentData = dataBytes.mid(i * MAX_PACKET_SIZE, currentSize);

        // 添加序列号到数据前面
        QByteArray sequenceData;
        sequenceData.resize(2);
        quint16 netSequence = htons(m_nSequence + i);
        memcpy(sequenceData.data(), &netSequence, 2);
        currentData.prepend(sequenceData);

        qint32 length = currentData.size() + 4;  // 数据长度(包含序列号) + 命令(2) + 用户ID(2)
        
        // 预分配空间
        QByteArray packet;
        packet.resize(2 + 4 + 2 + 2 + currentData.size() + 2); // 头部(2) + 长度(4) + 命令(2) + 用户ID(2) + 数据(含序列号) + 校验和(2)
        char* pData = packet.data();

        // 写入消息头（使用网络字节序）
        quint16 header = htons(MESSAGE_HEADER);
        memcpy(pData, &header, 2);
        pData += 2;

        // 写入长度
        quint32 netLength = htonl(length);
        memcpy(pData, &netLength, 4);
        pData += 4;

        // 写入命令
        quint16 netType = htons(type);
        memcpy(pData, &netType, 2);
        pData += 2;

        // 写入用户ID
        quint16 userId = htons(m_currentUserId);
        memcpy(pData, &userId, 2);
        pData += 2;

        // 写入数据（包含序列号）
        memcpy(pData, currentData.constData(), currentData.size());
        pData += currentData.size();

        // 计算并写入校验和
        quint16 sum = 0;
        for (int j = 0; j < currentData.size(); j++) {
            sum += (quint8)currentData[j];
        }
        quint16 netSum = htons(sum);
        memcpy(pData, &netSum, 2);

        // 缓存消息用于可能的重传
        if (m_messageCache.size() >= MAX_CACHE_SIZE) {
            // 移除最旧的消息
            m_messageCache.remove(m_messageCache.firstKey());
        }
        m_messageCache.insert(m_nSequence + i, CachedMessage{type, data, packet});

        packets.append(packet);
    }
    m_nSequence += totalPackets;

    return packets;
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
    QList<QByteArray> packets = createMessagePacket(messageType, message);
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
    QList<QByteArray> packets = createMessagePacket(YFile, data);
    qDebug() << "Sending file broadcast:" << data;
    for (const QByteArray& packet : packets) {
        m_socket->write(packet);
    }
}

void MessageBroadcaster::requestFileDownload(const QString &filename, const QString &sender)
{
    QString data = QString("%1 %2").arg(filename).arg(sender);
    QList<QByteArray> packets = createMessagePacket(YRecv, data);
    qDebug() << "Requesting file download:" << data;
    for (const QByteArray& packet : packets) {
        m_socket->write(packet);
    }
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

    // 读取序列号
    quint16 sequence;
    memcpy(&sequence, data.constData() + pos, 2);
    sequence = ntohs(sequence);
    pos += 2;

    // 数据内容长度 = length - 6（命令2+用户ID2+序列号2）
    QByteArray messageData = data.mid(pos, length - 6);

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
        handleRetransmissionRequest(messageData);
        return;
    }
    qDebug() << "begain process the msg";
    // 确保使用 UTF-8 解码
    QString message = QString::fromUtf8(messageData);
    
    switch (cmd) {
        case YConnect:
            emit userLoggedIn(message);
            m_userIdToName[userId] = message;
            break;
        case YUserList: {
            // message 形如 "1001|张三;1002|李四"
            QStringList users = message.split(";", QString::SkipEmptyParts);
            for (const QString& user : users) {
                QStringList pair = user.split("|");
                if (pair.size() == 2) {
                    bool ok = false;
                    int uid = pair[0].toInt(&ok);
                    if (ok) m_userIdToName[uid] = pair[1];
                }
            }
            break;
        }
        case YMsg:
            if (m_userIdToName.count(userId) > 0) {
                emit messageReceived(m_userIdToName[userId], message);
            }
            break;
        case YDisCon:
            if (m_userIdToName.count(userId) > 0) {
                emit userLoggedOut(m_userIdToName[userId]);
                m_userIdToName.erase(userId);
            }
            break;
        case YFile:
            {
                QStringList parts = message.split(" ");
                if (parts.size() >= 2) {
                    QString filename = parts[0];
                    qint64 filesize = parts[1].toLongLong();
                    if (m_userIdToName.count(userId) > 0) {
                        emit fileBroadcastReceived(m_userIdToName[userId], filename, filesize);
                    }
                }
            }
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
        default:
            qDebug() << "Unknown message type:" << cmd;
            break;
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
    emit disconnection(m_username);
}

void MessageBroadcaster::handleError(QAbstractSocket::SocketError socketError)
{
    m_isConnected = false;
    emit connectionError(m_socket->errorString());
}
