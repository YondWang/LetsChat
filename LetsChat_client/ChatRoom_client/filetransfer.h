#ifndef FILETRANSFER_H
#define FILETRANSFER_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>

class FileTransfer : public QObject
{
    Q_OBJECT
public:
    explicit FileTransfer(QObject *parent = nullptr);
    ~FileTransfer();

    // 文件传输接口
    void sendFile(const QString& filePath, QTcpSocket* socket);
    void downloadFile(const QString& filename, const QString &savepath, const QString &host, quint16 port);

signals:
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void uploadFinished();
    void downloadFinished();
    void error(const QString &errorMessage);
    void connectionStatus(const QString &status);
    void fileAckReceived(const QString& ackType);

public slots:
    void handleFileAck(const QString& ackType);
    void startUploadInThread();
    void startDownloadInThread();

private slots:
    void handleDownloadError(QAbstractSocket::SocketError socketError);
    void handleDownloadConnected();
    void handleDownloadDisconnected();
    void onUploadSocketReadyRead();
    void onDownloadSocketReadyRead();

private:
    // 下载相关
    QTcpSocket *m_downloadSocket;
    QFile *m_downloadFile;
    QMutex m_downloadMutex;
    qint64 m_downloadTotalBytes;
    qint64 m_downloadCurrentBytes;
    QString m_downloadServerFileName;
    
    // 上传相关
    QTcpSocket *m_uploadSocket;
    QFile *m_uploadFile;
    QMutex m_uploadMutex;
    qint64 m_uploadTotalBytes;
    qint64 m_uploadCurrentBytes;
    qint64 m_lastChunkSize;
    bool m_waitingFileAck;
    QString m_expectedAckType;
    QQueue<QByteArray> m_fileSendQueue;
    QQueue<int> m_fileSendDataLens;
    int m_fileSendIndex;
    
    // 线程相关
    QThread *m_uploadThread;
    QThread *m_downloadThread;
    
    // 传输参数
    QString m_uploadFilePath;
    QString m_downloadFilename;
    QString m_downloadHost;
    quint16 m_downloadPort;
    
    static const int BUFFER_SIZE = 8192;
    
    // 文件传输协议相关
    void sendFileStart(const QString& fileName, qint64 totalSize);
    void sendFileData();
    void receiveFileChunk();
    
    // 消息包创建
    QList<QByteArray> createMessagePacket(int type, const QByteArray &data);
    QList<QByteArray> createMessagePacket(int type, const QString &data);
    
    // 字节序转换
    void writeUint16(char*& pData, quint16 value);
    void writeUint32(char*& pData, quint32 value);
    
    static const quint16 MESSAGE_HEADER = 0xFEFF;
    static const int MAX_PACKET_SIZE = 8192;
    
public:
    QString m_lastSendFileName;
    qint64 m_nTotalSize;
};

#endif // FILETRANSFER_H 
