#ifndef NETINIT_H
#define NETINIT_H

#include <QThread>
#include <QTcpSocket>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <QString>

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

#include <QMessageBox>
#include <QObject>

#define PORT 2903
#define PACKAGE_SIZE 4096

//Receive and send thread
class ReceiveThread : public QThread{
    Q_OBJECT
public:
    explicit ReceiveThread(QTcpSocket* socket, QObject *parent = nullptr)
        : QThread(parent), socket(socket){}
    void run() override;

private:
    QTcpSocket *socket;
    QMutex mutex;

private slots:
    // slots func handle server message
    void onReadyRead();

signals:
    //notice received a new message
    void messageReceived(const QString &message);
};




class SendThread : public QThread{
    Q_OBJECT
public:
    explicit SendThread(QObject *parent = nullptr)
        : QThread(parent), socket(nullptr){}

    void setSocket(QTcpSocket *sock);
    void enqueueMessage(const QString &message);

protected:
    void run() override;

private:
    QTcpSocket *socket;
    QQueue<QString> messageQueue;
    QMutex mutex;
    QMutex sock_mutex;
    QWaitCondition condition;
    bool is_running;
};




//main thread : two thread fact in headfile
class NetInit : public QObject
{
    Q_OBJECT
public:
    explicit NetInit(QString usrName, QObject* parent = nullptr);

    ~NetInit();

    int initialize(const QString &host, unsigned short port = PORT);
    void sendMessage(const QString &message);
    QByteArray receiveMessage();

signals:
    void messageToShow(const QString &message);

private slots:
    void onMessageReceived(const QString &message);

private:
    QString usr_name;
    QTcpSocket *socket;
    ReceiveThread *receive_thread;
    SendThread *send_thread;
    bool is_running;

    //static const int PACKAGE_SIZE = 4096;
};



#endif // NETINIT_H
