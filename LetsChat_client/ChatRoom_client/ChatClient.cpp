#include "ChatClient.h"
#include <QDataStream>
#include <QDir>

ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_currentFileSize(0)
    , m_receivedBytes(0)
{
    connect(m_socket, &QTcpSocket::connected, this, &ChatClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &ChatClient::onError);
}

ChatClient::~ChatClient()
{
    disconnect();
}

bool ChatClient::connectToServer(const QString& host, quint16 port)
{
    m_socket->connectToHost(host, port);
    return m_socket->waitForConnected(5000);
}

void ChatClient::disconnect()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState)
            m_socket->waitForDisconnected(1000);
    }
}

void ChatClient::sendMessage(const QString& content)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    Packet packet = Packet::createChatMessage(m_username.toStdString(), content.toStdString());
    sendPacket(packet);
}

void ChatClient::sendFile(const QString& filePath)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit error("无法打开文件: " + filePath);
        return;
    }

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    uint32_t fileSize = file.size();

    // 发送文件开始包
    Packet startPacket = Packet::createFileStart(fileName.toStdString(), fileSize);
    sendPacket(startPacket);

    // 分块发送文件数据
    const uint32_t chunkSize = Packet::MAX_DATA_SIZE;
    uint32_t sequence = 0;
    while (!file.atEnd()) {
        QByteArray chunk = file.read(chunkSize);
        Packet dataPacket = Packet::createFileData(sequence++, chunk.data(), chunk.size());
        sendPacket(dataPacket);
    }

    // 发送文件结束包
    Packet endPacket = Packet::createFileEnd();
    sendPacket(endPacket);

    file.close();
}

void ChatClient::sendPacket(const Packet& packet)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.writeRawData(packet.getHeader(), packet.getHeaderSize());
    stream.writeRawData(packet.getData(), packet.getDataSize());
    m_socket->write(data);
}

void ChatClient::onConnected()
{
    emit connected();
}

void ChatClient::onDisconnected()
{
    emit disconnected();
}

void ChatClient::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    Packet packet;
    if (packet.parse(data.data(), data.size())) {
        switch (packet.getType()) {
            case Packet::Type::ChatMessage:
                handleChatMessage(packet);
                break;
            case Packet::Type::FileStart:
                handleFileStart(packet);
                break;
            case Packet::Type::FileData:
                handleFileData(packet);
                break;
            case Packet::Type::FileEnd:
                handleFileEnd(packet);
                break;
            case Packet::Type::SystemMessage:
                handleSystemMessage(packet);
                break;
        }
    }
}

void ChatClient::onError(QAbstractSocket::SocketError socketError)
{
    emit error(m_socket->errorString());
}

void ChatClient::handleChatMessage(const Packet& packet)
{
    std::string sender, content;
    packet.getChatMessage(sender, content);
    emit messageReceived(QString::fromStdString(sender), QString::fromStdString(content));
}

void ChatClient::handleFileStart(const Packet& packet)
{
    std::string fileName;
    uint32_t fileSize;
    packet.getFileStart(fileName, fileSize);

    m_currentFileName = QString::fromStdString(fileName);
    m_currentFileSize = fileSize;
    m_receivedBytes = 0;

    // 创建接收目录
    QDir dir("downloads");
    if (!dir.exists())
        dir.mkpath(".");

    // 打开文件准备接收
    m_receiveFile.setFileName(dir.filePath(m_currentFileName));
    if (!m_receiveFile.open(QIODevice::WriteOnly)) {
        emit error("无法创建文件: " + m_currentFileName);
        return;
    }

    emit fileTransferStarted(m_currentFileName, fileSize);
}

void ChatClient::handleFileData(const Packet& packet)
{
    if (!m_receiveFile.isOpen())
        return;

    const char* data;
    uint32_t size;
    packet.getFileData(data, size);

    m_receiveFile.write(data, size);
    m_receivedBytes += size;

    emit fileTransferProgress(m_receivedBytes, m_currentFileSize);
}

void ChatClient::handleFileEnd(const Packet& packet)
{
    if (m_receiveFile.isOpen()) {
        m_receiveFile.close();
        emit fileTransferCompleted();
    }
}

void ChatClient::handleSystemMessage(const Packet& packet)
{
    std::string content;
    packet.getSystemMessage(content);
    emit systemMessage(QString::fromStdString(content));
} 