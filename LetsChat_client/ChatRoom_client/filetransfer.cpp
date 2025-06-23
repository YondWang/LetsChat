#include "filetransfer.h"
#include <QDebug>
#include <QFileInfo>

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
    QMutexLocker locker(&m_uploadMutex);
    
    m_uploadFile = new QFile(filePath);
    if (!m_uploadFile->open(QIODevice::ReadOnly)) {
        emit error("无法打开文件进行上传");
        return;
    }

    m_uploadTotalBytes = m_uploadFile->size();
    
    QString msg = QString("FILE %1 %2").arg(QFileInfo(filePath).fileName()).arg(m_uploadTotalBytes);
    m_uploadSocket->connectToHost(host, port);
    m_uploadSocket->write(msg.toUtf8());
}

void FileTransfer::downloadFile(const QString &filename, const QString &host, quint16 port)
{
    QMutexLocker locker(&m_downloadMutex);
    
    m_downloadFile = new QFile(filename);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        emit error("无法创建文件进行下载");
        return;
    }

    QString msg = QString("REQ %1").arg(filename);
    m_downloadSocket->connectToHost(host, port);
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
    QString errorMsg = u8"上传文件时发生错误: " + m_uploadSocket->errorString();
    emit error(errorMsg);
}

void FileTransfer::handleDownloadError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    QString errorMsg = u8"下载文件时发生错误: " + m_downloadSocket->errorString();
    emit error(errorMsg);
} 
