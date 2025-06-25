#include "filetransfer.h"
#include <QDebug>
#include <QFileInfo>
#include <QDataStream>
#include <QThread>
#include <QMessageBox>

// 分包协议结构
struct FilePacketHeader {
    char filename[128];
    qint64 totalSize;
    qint64 offset;
    qint32 dataLen;
    bool isLast;
};

FileTransfer::FileTransfer(QObject *parent)
    : QObject(parent)
    , m_uploadSocket(new QTcpSocket(this))
    , m_downloadSocket(new QTcpSocket(this))
    , m_uploadFile(nullptr)
    , m_downloadFile(nullptr)
    , m_uploadTotalBytes(0)
    , m_downloadTotalBytes(0)
{
    connect(m_uploadSocket, &QTcpSocket::readyRead, this, &FileTransfer::handleUploadReadyRead);
    connect(m_downloadSocket, &QTcpSocket::readyRead, this, &FileTransfer::handleDownloadReadyRead);
    
    connect(m_uploadSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &FileTransfer::handleUploadError);
    connect(m_downloadSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &FileTransfer::handleDownloadError);
            
    // 添加连接状态的信号槽
    connect(m_uploadSocket, &QTcpSocket::connected, this, &FileTransfer::handleUploadConnected);
    connect(m_downloadSocket, &QTcpSocket::connected, this, &FileTransfer::handleDownloadConnected);
    connect(m_uploadSocket, &QTcpSocket::disconnected, this, &FileTransfer::handleUploadDisconnected);
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
}

void FileTransfer::uploadFile(const QString &filePath, const QString &host, quint16 port)
{
    if (m_uploadSocket->state() == QAbstractSocket::ConnectedState) {
        m_uploadSocket->disconnectFromHost();
    }
    
    qDebug() << "正在连接上传服务器..." << host << ":" << port;
    emit connectionStatus(QString("正在连接上传服务器 %1:%2").arg(host).arg(port));
    
    m_uploadSocket->connectToHost(host, port);
    if (!m_uploadSocket->waitForConnected(3000)) {
        QString errorMsg = QString("无法连接到上传服务器 %1:%2 - %3").arg(host).arg(port).arg(m_uploadSocket->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
        return;
    }
    
    FileSenderThread* sender = new FileSenderThread(filePath);
    connect(sender, &FileSenderThread::fileChunkReady, this, [this](const QByteArray& chunk){
        m_uploadSocket->write(chunk);
        m_uploadSocket->waitForBytesWritten(3000);
    });
    connect(sender, &QThread::finished, sender, &QObject::deleteLater);
    sender->start();
}

void FileTransfer::downloadFile(const QString &filename, const QString &host, quint16 port)
{
    QMutexLocker locker(&m_downloadMutex);
    
    m_downloadFile = new QFile(filename);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        QString errorMsg = QString("无法创建文件进行下载: %1").arg(m_downloadFile->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }

    qDebug() << "正在连接下载服务器..." << host << ":" << port;
    emit connectionStatus(QString("正在连接下载服务器 %1:%2").arg(host).arg(port));
    
    if (m_downloadSocket->state() == QAbstractSocket::ConnectedState) {
        m_downloadSocket->disconnectFromHost();
    }
    
    m_downloadSocket->connectToHost(host, port);
    if (!m_downloadSocket->waitForConnected(3000)) {
        QString errorMsg = QString("无法连接到下载服务器 %1:%2 - %3").arg(host).arg(port).arg(m_downloadSocket->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }

    QString msg = QString("REQ %1").arg(filename);
    m_downloadSocket->write(msg.toUtf8());
}

void FileTransfer::handleUploadReadyRead()
{
    QMutexLocker locker(&m_uploadMutex);
    
    if (!m_uploadFile) return;
    
    QByteArray data = m_uploadSocket->readAll();
    QString message = QString::fromUtf8(data);
    
    if (message.startsWith("ACP")) {
        sendFileChunk();
    }
}

void FileTransfer::handleDownloadReadyRead()
{
    QMutexLocker locker(&m_downloadMutex);
    
    if (!m_downloadFile) return;
    
    QByteArray data = m_downloadSocket->readAll();
    if (data.isEmpty()) return;
    
    QString message = QString::fromUtf8(data);
    if (message.startsWith("FILE")) {
        QStringList parts = message.split(" ");
        if (parts.size() >= 3) {
            m_downloadTotalBytes = parts[2].toLongLong();
            return;
        }
    }
    
    receiveFileChunk();
}

void FileTransfer::sendFileChunk()
{
    if (!m_uploadFile) return;
    
    QByteArray buffer = m_uploadFile->read(BUFFER_SIZE);
    if (!buffer.isEmpty()) {
        m_uploadSocket->write(buffer);
        emit uploadProgress(m_uploadFile->pos(), m_uploadTotalBytes);
    }
    
    if (m_uploadFile->atEnd()) {
        m_uploadFile->close();
        delete m_uploadFile;
        m_uploadFile = nullptr;
        emit uploadFinished();
    }
}

void FileTransfer::receiveFileChunk()
{
    if (!m_downloadFile) return;
    
    QByteArray data = m_downloadSocket->readAll();
    if (!data.isEmpty()) {
        qint64 written = m_downloadFile->write(data);
        if (written == -1) {
            emit error("error process download file");
            return;
        }
        
        emit downloadProgress(m_downloadFile->size(), m_downloadTotalBytes);
        
        if (m_downloadFile->size() >= m_downloadTotalBytes) {
            m_downloadFile->close();
            delete m_downloadFile;
            m_downloadFile = nullptr;
            emit downloadFinished();
        }
    }
}

void FileTransfer::handleUploadError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    QString errorMsg = "上传文件时发生错误: " + m_uploadSocket->errorString();
    emit error(errorMsg);
}

void FileTransfer::handleDownloadError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    QString errorMsg = "下载文件时发生错误: " + m_downloadSocket->errorString();
    emit error(errorMsg);
}

FileSenderThread::FileSenderThread(const QString& filePath, QObject* parent)
    : QThread(parent), m_filePath(filePath) {}

void FileSenderThread::run() {
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    qint64 totalSize = file.size();
    qint64 offset = 0;
    const int PACKET_SIZE = 4096;
    QByteArray filenameData = QFileInfo(m_filePath).fileName().toUtf8();
    filenameData = filenameData.leftJustified(128, '\0', true);

    while (!file.atEnd()) {
        QByteArray data = file.read(PACKET_SIZE);
        QByteArray packet;
        QDataStream stream(&packet, QIODevice::WriteOnly);
        stream.writeRawData(filenameData.constData(), 128);
        stream << totalSize << offset << (qint32)data.size() << (bool)file.atEnd();
        packet.append(data);
        emit fileChunkReady(packet);
        offset += data.size();
        msleep(2);
    }
    file.close();
}

void FileTransfer::handleUploadConnected()
{
    qDebug() << "upload server connected!";
    emit connectionStatus("upload server connected!");
}

void FileTransfer::handleDownloadConnected()
{
    qDebug() << "download server connected!";
    emit connectionStatus("download server connected!");
}

void FileTransfer::handleUploadDisconnected()
{
    qDebug() << "upload server disconnected!";
    emit connectionStatus("upload server disconnected!");
}

void FileTransfer::handleDownloadDisconnected()
{
    qDebug() << "download server disconnected!";
    emit connectionStatus("download server disconnected!");
}
