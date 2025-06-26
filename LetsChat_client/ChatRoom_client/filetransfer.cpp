#include "filetransfer.h"
#include <QDebug>
#include <QFileInfo>
#include <QDataStream>
#include <QThread>
#include <QMessageBox>
#include <QtEndian>
#include <Winsock2.h>
#include <QApplication>
#include <QDir>
#include <QNetworkProxy>
#pragma comment(lib, "ws2_32.lib")

FileTransfer::FileTransfer(QObject *parent)
    : QObject(parent)
    , m_downloadSocket(new QTcpSocket(this))
    , m_downloadFile(nullptr)
    , m_downloadTotalBytes(0)
    , m_uploadSocket(nullptr)
    , m_uploadFile(nullptr)
    , m_uploadTotalBytes(0)
    , m_uploadCurrentBytes(0)
    , m_lastChunkSize(0)
    , m_waitingFileAck(false)
    , m_fileSendIndex(0)
    , m_uploadThread(nullptr)
    , m_downloadThread(nullptr)
    , m_downloadPort(0)
{
    // 下载相关连接
    m_downloadSocket->setProxy(QNetworkProxy::NoProxy); // 禁用代理
    connect(m_downloadSocket, &QTcpSocket::readyRead, this, &FileTransfer::onDownloadSocketReadyRead);
    connect(m_downloadSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &FileTransfer::handleDownloadError);
    connect(m_downloadSocket, &QTcpSocket::connected, this, &FileTransfer::handleDownloadConnected);
    connect(m_downloadSocket, &QTcpSocket::disconnected, this, &FileTransfer::handleDownloadDisconnected);
}

FileTransfer::~FileTransfer()
{
    if (m_uploadFile) {
        m_uploadFile->close();
        delete m_uploadFile;
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
    }
    
    if (m_uploadThread) {
        m_uploadThread->quit();
        m_uploadThread->wait();
        delete m_uploadThread;
    }
    if (m_downloadThread) {
        m_downloadThread->quit();
        m_downloadThread->wait();
        delete m_downloadThread;
    }
}

void FileTransfer::sendFile(const QString& filePath, QTcpSocket* socket)
{
    m_uploadSocket = socket;
    m_uploadFilePath = filePath;
    m_lastSendFileName = QFileInfo(filePath).fileName();
    m_nTotalSize = QFileInfo(filePath).size();

    qDebug() << "开始文件上传:" << m_lastSendFileName << "大小:" << m_nTotalSize;

    // 连接readyRead槽，解析ACK
    connect(m_uploadSocket, &QTcpSocket::readyRead, this, &FileTransfer::onUploadSocketReadyRead);

    startUploadInThread();
}

void FileTransfer::startUploadInThread()
{
    QFile file(m_uploadFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件:" << m_uploadFilePath;
        emit error("无法打开文件: " + m_uploadFilePath);
        return;
    }
    
    m_uploadTotalBytes = m_nTotalSize;
    m_uploadCurrentBytes = 0;
    m_lastChunkSize = 0;
    m_fileSendQueue.clear();
    m_fileSendDataLens.clear();
    m_fileSendIndex = 0;
    m_waitingFileAck = false;
    m_expectedAckType.clear();

    // 1. 发送YFileStart（文本消息）
    sendFileStart(m_lastSendFileName, m_uploadTotalBytes);
    qDebug() << "[File] Send YFileStart:" << m_lastSendFileName << m_uploadTotalBytes;
    m_expectedAckType = "START";
    m_waitingFileAck = true;

    // 2. 预分包所有YFileData，并记录每个分包的数据长度
    while (!file.atEnd()) {
        QByteArray chunk = file.read(BUFFER_SIZE);
        QList<QByteArray> dataPackets = createMessagePacket(6, chunk); // YFileData = 6
        for (const QByteArray& packet : dataPackets) {
            m_fileSendQueue.enqueue(packet);
            m_fileSendDataLens.enqueue(chunk.size()); // 记录数据长度
        }
    }
    file.close();
}

void FileTransfer::downloadFile(const QString& filename, const QString& savepath, const QString& host, quint16 port)
{
    m_downloadFilename = savepath;
    m_downloadHost = host;
    m_downloadPort = port;
    m_downloadServerFileName = filename;
    qDebug() << "[downloadFile] filename param:" << filename << ", m_downloadServerFileName:" << m_downloadServerFileName;
    qDebug() << "begain download file:" << filename << "from" << host << ":" << m_downloadPort;
    startDownloadInThread();
}

void FileTransfer::startDownloadInThread()
{
    QMutexLocker locker(&m_downloadMutex);
    m_downloadFile = new QFile(m_downloadFilename);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        QString errorMsg = QString("无法创建文件进行下载: %1").arg(m_downloadFile->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }
    qDebug() << "正在连接下载服务器..." << m_downloadHost << ":" << m_downloadPort;
    emit connectionStatus(QString("正在连接下载服务器 %1:%2").arg(m_downloadHost).arg(m_downloadPort));
    if (m_downloadSocket->state() == QAbstractSocket::ConnectedState) {
        m_downloadSocket->disconnectFromHost();
    }
    m_downloadSocket->connectToHost(m_downloadHost, m_downloadPort);
    if (!m_downloadSocket->waitForConnected(3000)) {
        QString errorMsg = QString("无法连接到下载服务器 %1:%2 - %3").arg(m_downloadHost).arg(m_downloadPort).arg(m_downloadSocket->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }
    // 发送下载请求，使用YRecv协议包
    QList<QByteArray> packets = createMessagePacket(3, m_downloadServerFileName); // 3 = YRecv
    for (const QByteArray& packet : packets) {
        m_downloadSocket->write(packet);
        m_downloadSocket->waitForBytesWritten(1000);
    }
    qDebug() << "发送下载请求(YRecv包):" << m_downloadServerFileName;
}

void FileTransfer::sendFileStart(const QString& fileName, qint64 totalSize)
{
    QString data = fileName + "|" + QString::number(totalSize);
    QList<QByteArray> packets = createMessagePacket(5, data); // YFileStart = 5
    qDebug() << "发送文件开始包:" << data << "包数量:" << packets.size();
    for (const QByteArray& packet : packets) {
        qint64 written = m_uploadSocket->write(packet);
        m_uploadSocket->waitForBytesWritten(1000);
        qDebug() << "发送包大小:" << written << "实际包大小:" << packet.size();
    }
}

void FileTransfer::sendFileData()
{
    if (m_fileSendQueue.isEmpty()) {
        // 发送YFileEnd，内容为文件名
        QList<QByteArray> packets = createMessagePacket(7, m_lastSendFileName); // YFileEnd = 7
        for (const QByteArray& packet : packets) {
            m_uploadSocket->write(packet);
            m_uploadSocket->waitForBytesWritten(1000);
        }
        qDebug() << "[File] Send YFileEnd:" << m_lastSendFileName;
        m_expectedAckType = "END";
        m_waitingFileAck = true;
        return;
    }
    
    QByteArray packet = m_fileSendQueue.dequeue();
    m_lastChunkSize = m_fileSendDataLens.dequeue(); // 只记录数据部分长度
    m_uploadSocket->write(packet);
    m_uploadSocket->waitForBytesWritten(1000);
    qDebug() << QString("[File] Send YFileData chunk %1, size %2").arg(m_fileSendIndex++).arg(m_lastChunkSize);
    m_expectedAckType = "DATA";
    m_waitingFileAck = true;
}

void FileTransfer::handleFileAck(const QString& ackType)
{
    qDebug() << "收到文件确认信号:" << ackType << "等待类型:" << m_expectedAckType << "等待状态:" << m_waitingFileAck;
    if (!m_waitingFileAck) return;
    if (ackType == m_expectedAckType) {
        m_waitingFileAck = false;
        if (ackType == "START") {
            qDebug() << "start send file data";
            // 开始分包发送
            sendFileData();
        } else if (ackType == "DATA") {
            m_uploadCurrentBytes += m_lastChunkSize;
            emit uploadProgress(m_uploadCurrentBytes, m_uploadTotalBytes);
            qDebug() << QString("transfer byte info: %1/%2").arg(m_uploadCurrentBytes).arg(m_uploadTotalBytes);
            sendFileData();
        } else if (ackType == "END") {
            qDebug() << "[File] complete send file";
            emit uploadFinished();
        }
    } else {
        qDebug() << "文件确认类型不匹配:" << ackType << "!=" << m_expectedAckType;
    }
}

void FileTransfer::handleDownloadError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    QString errorMsg = "下载文件时发生错误: " + m_downloadSocket->errorString();
    qDebug() << errorMsg;
    emit error(errorMsg);
}

void FileTransfer::handleDownloadConnected()
{
    qDebug() << "download server connected!";
    emit connectionStatus("download server connected!");
}

void FileTransfer::handleDownloadDisconnected()
{
    qDebug() << "download server disconnected!";
    emit connectionStatus("download server disconnected!");
}

// 写入大端序的16位整数
void FileTransfer::writeUint16(char*& pData, quint16 value) {
    pData[0] = (value >> 8) & 0xFF;
    pData[1] = value & 0xFF;
    pData += 2;
}

// 写入大端序的32位整数
void FileTransfer::writeUint32(char*& pData, quint32 value) {
    pData[0] = (value >> 24) & 0xFF;
    pData[1] = (value >> 16) & 0xFF;
    pData[2] = (value >> 8) & 0xFF;
    pData[3] = value & 0xFF;
    pData += 4;
}

QList<QByteArray> FileTransfer::createMessagePacket(int type, const QByteArray &data)
{
    QList<QByteArray> packets;
    int totalSize = data.size();
    int offset = 0;
    const int DATA_MAX = MAX_PACKET_SIZE; // 只分数据部分
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
        quint16 userId = htons(0); // 文件传输不需要用户ID
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

QList<QByteArray> FileTransfer::createMessagePacket(int type, const QString &data)
{
    return createMessagePacket(type, data.toUtf8());
}

// 新增：上传socket数据包解析
void FileTransfer::onUploadSocketReadyRead()
{
    QByteArray data = m_uploadSocket->readAll();
    int pos = 0;
    while (data.size() - pos >= 12) { // 至少有头部
        quint16 header;
        memcpy(&header, data.constData() + pos, 2);
        header = ntohs(header);
        if (header != MESSAGE_HEADER) {
            pos++;
            continue;
        }
        quint32 length;
        memcpy(&length, data.constData() + pos + 2, 4);
        length = ntohl(length);
        if (data.size() - pos < (int)(length + 8)) break; // 不完整
        quint16 cmd;
        memcpy(&cmd, data.constData() + pos + 6, 2);
        cmd = ntohs(cmd);
        //quint16 userId; // 跳过
        //memcpy(&userId, data.constData() + pos + 8, 2);
        QByteArray msgData = data.mid(pos + 10, length - 4);
        // 校验和
        quint16 sum;
        memcpy(&sum, data.constData() + pos + 10 + msgData.size(), 2);
        sum = ntohs(sum);
        quint16 calcSum = 0;
        for (int i = 0; i < msgData.size(); ++i) calcSum += (quint8)msgData[i];
        if (sum != calcSum) {
            pos += length + 8;
            continue;
        }
        if (cmd == 8) { // YFileAck
            QString ackType = QString::fromUtf8(msgData);
            handleFileAck(ackType);
        }
        pos += length + 8;
    }
}

// 新增：下载socket数据包处理
void FileTransfer::onDownloadSocketReadyRead()
{
    QMutexLocker locker(&m_downloadMutex);
    if (!m_downloadFile) { qDebug() << "[Download] 没有打开的下载文件，直接返回"; return; }
    QByteArray data = m_downloadSocket->readAll();
    qDebug() << "[Download] readyRead触发, data size:" << data.size();
    int pos = 0;
    int loopCount = 0;
    while (data.size() - pos >= 12) { // 至少有头部
        loopCount++;
        qDebug() << "[Download] while " << loopCount << " times, pos=" << pos << ", remain data=" << (data.size() - pos);
        quint16 header;
        memcpy(&header, data.constData() + pos, 2);
        header = ntohs(header);
        if (header != MESSAGE_HEADER) {
            qDebug() << "[Download] error head, skip 1 byte";
            pos++;
            continue;
        }
        quint32 length;
        memcpy(&length, data.constData() + pos + 2, 4);
        length = ntohl(length);
        if (data.size() - pos < (int)(length + 8)) {
            qDebug() << "[Download] 数据不完整, break";
            break; // 不完整
        }
        quint16 cmd;
        memcpy(&cmd, data.constData() + pos + 6, 2);
        cmd = ntohs(cmd);
        QByteArray msgData = data.mid(pos + 10, length - 4);
        qDebug() << "[Download] 解析包 cmd=" << cmd << " size=" << msgData.size();
        if (cmd == 5) { // YFileStart
            qDebug() << "[Download] 进入YFileStart分支";
            QString info = QString::fromUtf8(msgData);
            QStringList parts = info.split("|");
            if (parts.size() == 2) {
                m_downloadServerFileName = parts[0];
                m_downloadTotalBytes = parts[1].toLongLong();
                m_downloadCurrentBytes = 0;
                qDebug() << "[Download] 收到YFileStart, 文件名:" << m_downloadServerFileName << ", 总字节:" << m_downloadTotalBytes;
                // 回ACK(START)
                QList<QByteArray> ack = createMessagePacket(8, (QString)"START");
                for (const QByteArray& packet : ack) {
                    m_downloadSocket->write(packet);
                    m_downloadSocket->waitForBytesWritten(1000);
                }
                m_downloadSocket->flush(); // 强制flush
                qDebug() << "[Download] 发送ACK(START)";
            }
        } else if (cmd == 6) { // YFileData
            qDebug() << "[Download] 进入YFileData分支, 写入文件";
            m_downloadFile->write(msgData);
            m_downloadCurrentBytes += msgData.size();
            emit downloadProgress(m_downloadCurrentBytes, m_downloadTotalBytes);
            qDebug() << QString("[Download] 收到YFileData, 当前进度: %1/%2").arg(m_downloadCurrentBytes).arg(m_downloadTotalBytes);
            // 回ACK(DATA)
            QList<QByteArray> ack = createMessagePacket(8, (QString)"DATA");
            for (const QByteArray& packet : ack) {
                m_downloadSocket->write(packet);
                m_downloadSocket->waitForBytesWritten(1000);
            }
            m_downloadSocket->flush(); // 强制flush
            qDebug() << "[Download] 发送ACK(DATA)";
        } else if (cmd == 7) { // YFileEnd
            qDebug() << "[Download] 进入YFileEnd分支, 关闭文件";
            m_downloadFile->flush();
            m_downloadFile->close();
            delete m_downloadFile;
            m_downloadFile = nullptr;
            qDebug() << "[Download] 收到YFileEnd, 文件下载完成";
            // 回ACK(END)
            QList<QByteArray> ack = createMessagePacket(8, (QString)"END");
            for (const QByteArray& packet : ack) {
                m_downloadSocket->write(packet);
                m_downloadSocket->waitForBytesWritten(1000);
            }
            m_downloadSocket->flush(); // 强制flush
            qDebug() << "[Download] 发送ACK(END)";
            emit downloadFinished();
        } else {
            qDebug() << "[Download] 未知命令:" << cmd;
        }
        pos += (length + 8);
    }
    qDebug() << "[Download] onDownloadSocketReadyRead处理结束, pos=" << pos << ", 总数据=" << data.size();
}
