#include "netinit.h"

#include <QAbstractButton>

NetInit::NetInit(QString usrName, QObject *parent) :
    QObject(parent), m_sUsrName(usrName), socket(new QTcpSocket(this)),
    receive_thread(nullptr), send_thread(nullptr),
    is_running(true) {}

NetInit::~NetInit()
{
    is_running = false;

        if(socket) {
            socket->disconnectFromHost();
            if(socket->state() != QAbstractSocket::UnconnectedState)
                socket->waitForDisconnected();
        }

        if(receive_thread) {
            receive_thread->quit();
            receive_thread->wait();
            delete receive_thread;
        }

        if(send_thread) {
            send_thread->quit();
            send_thread->wait();
            delete send_thread;
        }
}

int NetInit::initialize(const QString &host, unsigned short port)
{
    socket->connectToHost(host, port);
    if(!socket->waitForConnected(3000)){
        //QMessageBox::warning(nullptr, u8"connect problem", u8"Connection failed！", QMessageBox::Ok, QMessageBox::Close);
        return -1;
    }
    QMessageBox::information(nullptr, u8"congratualtion!", u8"connect success!!");

    receive_thread = new ReceiveThread(socket);
    connect(receive_thread, &ReceiveThread::messageReceived, this, &NetInit::onMessageReceived);
    receive_thread->start();

    send_thread = new SendThread();
    send_thread->setSocket(socket);
    send_thread->start();

    const QString logIden = "LOG " + m_sUsrName;
    sendMessage(logIden);

    return 1;
}

//NetInit
void NetInit::sendMessage(const QString &message)
{
    if(send_thread){
        qDebug() << message;
        send_thread->enqueueMessage(message);
    }
}

QByteArray NetInit::receiveMessage()
{
    QByteArray buffer;
    if(socket && socket->waitForReadyRead(1000)) {
        buffer = socket->readAll();
    }
    return buffer;
}

void NetInit::onMessageReceived(const QString &message){
    qDebug() << "received message:" << message;     //接收消息
    emit messageToShow(message);
}

//ReceiveThread
void ReceiveThread::run(){        //rewrite QThread::run
    connect(socket, &QTcpSocket::readyRead, this , &ReceiveThread::onReadyRead, Qt::QueuedConnection);
    exec();
}

void ReceiveThread::onReadyRead() {
    QMutexLocker locker(&mutex);
    QByteArray data = socket->readAll();                    // read data from server
    if(!data.isEmpty()){
        emit messageReceived(QString::fromUtf8(data));          // to string and put msg to main thread
    }
}

//SendThread
void SendThread::setSocket(QTcpSocket *sock){
    QMutexLocker locker(&mutex);
    this->socket = sock;
    //socket->moveToThread(this);
}

void SendThread::enqueueMessage(const QString &message){
    QMutexLocker locker(&mutex);        //to keep thread safe
    messageQueue.enqueue(message);      //enqueue the msg
    condition.wakeOne();                //wake waiting thread, notice have new msg
}

void SendThread::run(){
    while (is_running) {
        QString message;

        QMutexLocker locker(&mutex);
        if(message.isEmpty()){
            condition.wait(&mutex);
            if(!is_running) break;
        }
        if(!messageQueue.isEmpty()){
            message = messageQueue.dequeue();
        }

        if(!message.isEmpty() && socket && socket->state() == QAbstractSocket::ConnectedState){
            QMutexLocker socketLocker(&sock_mutex);
            int bytes_written = socket->write(message.toUtf8());
            bool flushed = socket->flush();

            if(bytes_written == -1 || !flushed){
                qDebug() << "error!";
            }
        }
    }
}
