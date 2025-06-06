#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include "Packet.h"

// 消息包头部结构
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic;        // 魔数，用于校验包的有效性
    uint16_t type;         // 消息类型：1=私聊，2=群发，3=系统消息
    uint16_t senderLen;    // 发送者名称长度
    uint16_t contentLen;   // 消息内容长度
    uint16_t targetLen;    // 目标接收者长度
};
#pragma pack(pop)

// 消息包结构
struct ChatMessage {
    uint16_t type;           // 消息类型：1=私聊，2=群发，3=系统消息
    QString sender;          // 发送者
    QString content;         // 消息内容
    QString target;          // 目标接收者

    // 序列化到二进制
    QByteArray toBinary() const {
        MessageHeader header;
        header.magic = 0x12345678;  // 魔数
        header.type = type;
        header.senderLen = sender.toUtf8().length();
        header.contentLen = content.toUtf8().length();
        header.targetLen = target.toUtf8().length();

        QByteArray data;
        data.append(reinterpret_cast<char*>(&header), sizeof(header));
        data.append(sender.toUtf8());
        data.append(content.toUtf8());
        data.append(target.toUtf8());
        return data;
    }

    // 从二进制反序列化
    static ChatMessage fromBinary(const QByteArray& binary) {
        ChatMessage msg;
        if (binary.length() < sizeof(MessageHeader)) {
            return msg;
        }

        const MessageHeader* header = reinterpret_cast<const MessageHeader*>(binary.data());
        if (header->magic != 0x12345678) {
            return msg;
        }

        size_t offset = sizeof(MessageHeader);
        msg.type = header->type;
        
        if (offset + header->senderLen <= binary.length()) {
            msg.sender = QString::fromUtf8(binary.mid(offset, header->senderLen));
            offset += header->senderLen;
        }
        
        if (offset + header->contentLen <= binary.length()) {
            msg.content = QString::fromUtf8(binary.mid(offset, header->contentLen));
            offset += header->contentLen;
        }
        
        if (offset + header->targetLen <= binary.length()) {
            msg.target = QString::fromUtf8(binary.mid(offset, header->targetLen));
        }

        return msg;
    }
};

class ChatClient : public QObject
{
    Q_OBJECT
public:
    explicit ChatClient(QObject *parent = nullptr);
    ~ChatClient();

    bool connectToServer(const QString& host, quint16 port);
    void disconnect();
    void sendMessage(const QString& content);
    void sendFile(const QString& filePath);
    void setUsername(const QString& username) { m_username = username; }

signals:
    void messageReceived(const QString& sender, const QString& content);
    void systemMessage(const QString& content);
    void fileTransferStarted(const QString& fileName, uint32_t fileSize);
    void fileTransferProgress(uint32_t bytesReceived, uint32_t totalBytes);
    void fileTransferCompleted();
    void connected();
    void disconnected();
    void error(const QString& error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);

private:
    void sendPacket(const Packet& packet);
    void handleChatMessage(const Packet& packet);
    void handleFileStart(const Packet& packet);
    void handleFileData(const Packet& packet);
    void handleFileEnd(const Packet& packet);
    void handleSystemMessage(const Packet& packet);

    QTcpSocket* m_socket;
    QString m_username;
    QString m_currentFileName;
    uint32_t m_currentFileSize;
    uint32_t m_receivedBytes;
    QFile m_receiveFile;
}; 