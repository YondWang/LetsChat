#ifndef FILETRANSFER_H
#define FILETRANSFER_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QThread>
#include <QMutex>

class FileTransfer : public QObject
{
    Q_OBJECT
public:
    explicit FileTransfer(QObject *parent = nullptr);
    ~FileTransfer();

    void uploadFile(const QString &filePath, const QString &host, quint16 port);
    void downloadFile(const QString &filename, const QString &host, quint16 port);

signals:
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void uploadFinished();
    void downloadFinished();
    void error(const QString &errorMessage);
    void connectionStatus(const QString &status);

private slots:
    void handleUploadReadyRead();
    void handleDownloadReadyRead();
    void handleUploadError(QAbstractSocket::SocketError socketError);
    void handleDownloadError(QAbstractSocket::SocketError socketError);
    void handleUploadConnected();
    void handleDownloadConnected();
    void handleUploadDisconnected();
    void handleDownloadDisconnected();

private:
    QTcpSocket *m_uploadSocket;
    QTcpSocket *m_downloadSocket;
    QFile *m_uploadFile;
    QFile *m_downloadFile;
    QMutex m_uploadMutex;
    QMutex m_downloadMutex;
    qint64 m_uploadTotalBytes;
    qint64 m_downloadTotalBytes;
    
    static const int BUFFER_SIZE = 8192;
    void sendFileChunk();
    void receiveFileChunk();
};

class FileSenderThread : public QThread {
    Q_OBJECT
public:
    FileSenderThread(const QString& filePath, QObject* parent = nullptr);
    void run() override;
signals:
    void fileChunkReady(const QByteArray& chunk);
private:
    QString m_filePath;
};

#endif // FILETRANSFER_H 
