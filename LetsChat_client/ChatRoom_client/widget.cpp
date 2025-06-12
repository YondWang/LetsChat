#include "widget.h"
#include "ui_widget.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>

Widget::Widget(QString usrName, QWidget* parent)
	: QWidget(parent)
	, m_username(usrName)
    , ui(new Ui::Widget)
{
	ui->setupUi(this);

	m_broadcaster = new MessageBroadcaster(this);
	m_fileTransfer = new FileTransfer(this);

	setupConnections();

	// 连接到服务器
    m_broadcaster->connectToServer("127.0.0.1", 2903);
    qDebug() << "connectToServer\n";

	// 发送登录广播
	m_broadcaster->sendLoginBroadcast(m_username);
    qDebug() << "broadcast login msg\n";

	// 设置窗口标题
    setWindowTitle(u8"聊天室 - " + m_username);
}

Widget::~Widget()
{
	delete ui;
}

void Widget::setupConnections()
{
	// 消息广播器信号连接
	connect(m_broadcaster, &MessageBroadcaster::messageReceived,
		this, &Widget::handleMessageReceived);
	connect(m_broadcaster, &MessageBroadcaster::userLoggedIn,
		this, &Widget::handleUserLoggedIn);
	connect(m_broadcaster, &MessageBroadcaster::userLoggedOut,
		this, &Widget::handleUserLoggedOut);
	connect(m_broadcaster, &MessageBroadcaster::fileBroadcastReceived,
		this, &Widget::handleFileBroadcastReceived);
	connect(m_broadcaster, &MessageBroadcaster::fileDownloadRequested,
		this, &Widget::handleFileDownloadRequested);
	connect(m_broadcaster, &MessageBroadcaster::connectionError,
		this, &Widget::handleConnectionError);

	// 文件传输信号连接
	connect(m_fileTransfer, &FileTransfer::uploadProgress,
		this, &Widget::handleUploadProgress);
	connect(m_fileTransfer, &FileTransfer::downloadProgress,
		this, &Widget::handleDownloadProgress);
	connect(m_fileTransfer, &FileTransfer::uploadFinished,
		this, &Widget::handleUploadFinished);
	connect(m_fileTransfer, &FileTransfer::downloadFinished,
		this, &Widget::handleDownloadFinished);
	connect(m_fileTransfer, &FileTransfer::error,
		this, &Widget::handleFileTransferError);
}

QString Widget::usrNameGetter()
{
	return m_username;
}

void Widget::on_close_pb_clicked()
{
	close();
}

void Widget::on_send_pb_clicked()
{
	QString message = ui->send_te->toPlainText();
    if (message.isEmpty()) {
        QMessageBox::information(this, u8"notice!!", u8"you can`t send a NULL message!");
        return;
    }

	m_broadcaster->sendMessage(message);
    displayMessage("yourself", message);
	ui->send_te->clear();
}

void Widget::on_sendFile_pb_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, u8"选择要发送的文件");
	if (filePath.isEmpty()) return;

	QFileInfo fileInfo(filePath);
	m_broadcaster->sendFileBroadcast(fileInfo.fileName(), fileInfo.size());

    m_uploadProgress = new QProgressDialog(u8"正在上传文件...", u8"取消", 0, 100, this);
	m_uploadProgress->setWindowModality(Qt::WindowModal);
	m_uploadProgress->setAutoClose(true);
	m_uploadProgress->setAutoReset(true);

	m_fileTransfer->uploadFile(filePath, "localhost", 8888);
}

void Widget::handleMessageReceived(const QString& sender, const QString& message)
{
	displayMessage(sender, message);
}

void Widget::handleUserLoggedIn(const QString& username)
{
	if (username != m_username) {
        QString msg = username;
        msg += u8" 加入了聊天室";
        displayMessage(u8"系统", msg);
		updateUserList();
	}
}

void Widget::handleUserLoggedOut(const QString& username)
{
    QString msg = username;
    msg += u8" 离开了聊天室";
    displayMessage(u8"系统", msg);
	updateUserList();
}

void Widget::handleFileBroadcastReceived(const QString& sender, const QString& filename, qint64 filesize)
{
	displayFileMessage(sender, filename, filesize);
}

void Widget::handleFileDownloadRequested(const QString& filename, const QString& sender)
{
    QString savePath = QFileDialog::getSaveFileName(this, u8"保存文件", filename);
	if (savePath.isEmpty()) return;

    m_downloadProgress = new QProgressDialog(u8"正在下载文件...", u8"取消", 0, 100, this);
	m_downloadProgress->setWindowModality(Qt::WindowModal);
	m_downloadProgress->setAutoClose(true);
	m_downloadProgress->setAutoReset(true);

	m_fileTransfer->downloadFile(savePath, "localhost", 8888);
}

void Widget::handleConnectionError(const QString& error)
{
    QMessageBox::critical(this, u8"connect error!", error);
}

void Widget::handleUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
	if (m_uploadProgress) {
		int percentage = (bytesSent * 100) / bytesTotal;
		m_uploadProgress->setValue(percentage);
	}
}

void Widget::handleDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
	if (m_downloadProgress) {
		int percentage = (bytesReceived * 100) / bytesTotal;
		m_downloadProgress->setValue(percentage);
	}
}

void Widget::handleUploadFinished()
{
	if (m_uploadProgress) {
		m_uploadProgress->close();
		m_uploadProgress = nullptr;
	}
}

void Widget::handleDownloadFinished()
{
	if (m_downloadProgress) {
		m_downloadProgress->close();
		m_downloadProgress = nullptr;
	}
    QMessageBox::information(this, u8"download done", u8"completed downloadfile");
}

void Widget::handleFileTransferError(const QString& errorMessage)
{
    QMessageBox::critical(this, u8"file transfer error", errorMessage);
}

void Widget::updateUserList()
{
	// TODO: 实现用户列表更新
}

void Widget::displayMessage(const QString& sender, const QString& message)
{
	QString displayText = QString("[%1] %2: %3")
		.arg(QTime::currentTime().toString("hh:mm:ss"))
		.arg(sender)
		.arg(message);

	ui->chatWindow_lw->addItem(displayText);
	ui->chatWindow_lw->scrollToBottom();
}

void Widget::displayFileMessage(const QString& sender, const QString& filename, qint64 filesize)
{
	QString sizeStr;
	if (filesize < 1024) {
		sizeStr = QString::number(filesize) + u8" B";
	}
	else if (filesize < 1024 * 1024) {
		sizeStr = QString::number(filesize / 1024.0, 'f', 2) + u8" KB";
	}
	else {
		sizeStr = QString::number(filesize / (1024.0 * 1024.0), 'f', 2) + u8" MB";
	}

    QString timeStr = QTime::currentTime().toString("hh:mm:ss");
    QString displayText = QString(u8"[%1] %2 发送了文件: %3 (%4)")
		.arg(timeStr)
		.arg(sender)
		.arg(filename)
		.arg(sizeStr);

	QListWidgetItem* item = new QListWidgetItem(displayText);
	item->setData(Qt::UserRole, QVariant::fromValue(QPair<QString, QString>(sender, filename)));
	ui->chatWindow_lw->addItem(item);
	ui->chatWindow_lw->scrollToBottom();
}









