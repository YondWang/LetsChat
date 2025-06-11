#include "messagebroadcaster.h"
#include <QDebug>

// 消息头部标识
const unsigned short MESSAGE_HEADER = 0xFEFF;

MessageBroadcaster::MessageBroadcaster(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_isConnected(false)
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
    QByteArray dataBytes = data.toUtf8();
    quint32 length = dataBytes.size() + 4; // 4 = 命令类型(2字节) + 用户ID(2字节)
    
    // 预分配空间
    QByteArray packet;
    packet.resize(2 + 4 + 2 + 2 + dataBytes.size() + 2); // 头部(2) + 长度(4) + 命令(2) + 用户ID(2) + 数据 + 校验和(2)
    char* pData = packet.data();
    
    // 写入消息头
    writeUint16(pData, MESSAGE_HEADER);
    writeUint32(pData, length);
    writeUint16(pData, type);
    writeUint16(pData, 0); // 用户ID暂时使用0
    
    // 写入数据
    memcpy(pData, dataBytes.constData(), dataBytes.size());
    pData += dataBytes.size();
    
    // 计算并写入校验和
    quint16 sum = 0;
    for (int i = 0; i < dataBytes.size(); i++) {
        sum += (quint8)dataBytes[i];
    }
    writeUint16(pData, sum);

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
    qDebug() << "Received raw data:" << data;
    m_buffer.append(data);
    
    while (m_buffer.size() >= 2) {
        // 查找消息头部
        int headerPos = -1;
        for (int i = 0; i <= m_buffer.size() - 2; i++) {
            if ((quint8)m_buffer[i] == ((MESSAGE_HEADER >> 8) & 0xFF) &&
                (quint8)m_buffer[i + 1] == (MESSAGE_HEADER & 0xFF)) {
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
        
        if (m_buffer.size() < 6) return;
        
        int pos = 0;
        quint16 header = readUint16(m_buffer, pos);
        quint32 length = readUint32(m_buffer, pos);
        
        if (m_buffer.size() < length + 8) return;
        
        QByteArray message = m_buffer.left(length + 8);
        m_buffer.remove(0, length + 8);
        
        parseMessage(message);
    }
}

void MessageBroadcaster::parseMessage(const QByteArray &data)
{
    if (data.size() < 8) return;

    int pos = 0;
    quint16 header = readUint16(data, pos);
    if (header != MESSAGE_HEADER) return;

    quint32 length = readUint32(data, pos);
    if (length > data.size() - 6) return;

    quint16 cmd = readUint16(data, pos);
    quint16 userId = readUint16(data, pos);

    QByteArray messageData = data.mid(pos, length - 4);
    QString message = QString::fromUtf8(messageData);
    qDebug() << "Parsed message:" << message;

    int tempPos = pos + messageData.size();
    quint16 sum = readUint16(data, tempPos);

    quint16 calculatedSum = 0;
    for (int i = 0; i < messageData.size(); i++) {
        calculatedSum += (quint8)messageData[i];
    }
    if (calculatedSum != sum) return;

    switch (cmd) {
        case YMsg:
            emit messageReceived(QString::number(userId), message);
            break;
        case YConnect:
            if (message.contains("joined")) {
                emit userLoggedIn(message.split(" ")[0]);
            } else if (message.contains("left")) {
                emit userLoggedOut(message.split(" ")[0]);
            }
            break;
        case YFile:
            {
                QStringList parts = message.split(" ");
                if (parts.size() >= 2) {
                    emit fileBroadcastReceived(QString::number(userId), parts[0], parts[1].toLongLong());
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
    }
}

void MessageBroadcaster::handleConnected()
{
    m_isConnected = true;
    emit connectionError("");
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
