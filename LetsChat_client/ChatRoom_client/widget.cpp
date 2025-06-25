#include "widget.h"
#include "ui_widget.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

Widget::Widget(QString usrName, QWidget* parent)
	: QWidget(parent)
	, m_username(usrName)
    , ui(new Ui::Widget)
{
	ui->setupUi(this);

    m_broadcaster = new MessageBroadcaster(this);
	m_fileTransfer = new FileTransfer(this);

	setupConnections();
    ui->connectStatus_lb->setText("connectStatus:false!!!");

	// 服务器地址和端口
    //const QString SERVER_HOST = "172.26.220.193";
    const QString SERVER_HOST = "127.0.0.1";
	const quint16 SERVER_PORT = 2903;

	// 连接到服务器
    m_broadcaster->connectToServer(SERVER_HOST, SERVER_PORT);
    qDebug() << "connectToServer to" << SERVER_HOST << ":" << SERVER_PORT << "\n";

	// 发送登录广播
    m_broadcaster->sendDataByPackCut(m_username, MessageBroadcaster::YConnect);
    qDebug() << "broadcast login msg\n";

	// 设置窗口标题
    ui->userName_tl->setText(m_username);
    setWindowTitle("聊天室_u:" + m_username);
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
    connect(m_broadcaster, &MessageBroadcaster::disconnection,
        this, &Widget::handleDisconnected);
	connect(m_broadcaster, &MessageBroadcaster::connectionError,
		this, &Widget::handleConnectionError);
    connect(m_broadcaster, &MessageBroadcaster::connected, this, [this](){
        ui->connectStatus_lb->setText("connectStatus:true!!!");
    });

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
        QMessageBox::information(this, "notice!!", "you can`t send a NULL message!");
        return;
    }
    m_broadcaster->sendDataByPackCut(message);
    displayMessage("yourself", message);
    ui->send_te->clear();
}

void Widget::on_sendFile_pb_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择要发送的文件");
	if (filePath.isEmpty()) return;

	// 调用新的、统一的文件发送接口
	if (m_broadcaster) {
		m_broadcaster->sendFile(filePath);
	}

	// 提示用户文件正在后台发送
    QMessageBox::information(this, "file transfing", "file is upload in background");
}

void Widget::handleMessageReceived(const QString& username, const QString& message)
{
    QString strusername = m_onlineUserName[username.toInt()];
    displayMessage(strusername, message);
}

void Widget::handleUserLoggedIn(int userId, const QString &userName)
{
    m_onlineUserName[userId] = userName;
    if (userId != m_myUserId) {
        QString msg = userName + "_加入了聊天室";
        displayMessage("系统", msg);
    }
    updateUserList();
}

void Widget::handleUserLoggedOut(int userId)
{
    QString userName = m_onlineUserName.value(userId, QString::number(userId));
    QString msg = userName + "_离开了聊天室";
    displayMessage("系统", msg);
    m_onlineUserName.remove(userId);
    updateUserList();
}

void Widget::handleFileBroadcastReceived(const QString& sender, const QString& filename, qint64 filesize)
{
    displayFileMessage(sender, filename, filesize);
}

void Widget::handleFileDownloadRequested(const QString& filename, const QString& sender)
{
    QString savePath = QFileDialog::getSaveFileName(this, "保存文件", filename);
	if (savePath.isEmpty()) return;

    m_downloadProgress = new QProgressDialog("正在下载文件...", "取消", 0, 100, this);
	m_downloadProgress->setWindowModality(Qt::WindowModal);
	m_downloadProgress->setAutoClose(true);
	m_downloadProgress->setAutoReset(true);

    m_fileTransfer->downloadFile(savePath, "localhost", 8888);
}

void Widget::handleDisconnected(const QString &username)
{
    QMessageBox::information(this, "disconnected!", username + " is disconnected from server!");
}

void Widget::handleConnectionError(const QString& error)
{
    QMessageBox::critical(this, "connect error!", error);
    ui->connectStatus_lb->setText("connectStatus:false!!!");
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
    QMessageBox::information(this, "download done", u8"completed downloadfile");
}

void Widget::handleFileTransferError(const QString& errorMessage)
{
    QMessageBox::critical(this, "file transfer error", errorMessage);
}

void Widget::updateUserList()
{

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
    // 创建文件消息容器
    QWidget* fileContainer = new QWidget(this);
    QHBoxLayout* fileLayout = new QHBoxLayout(fileContainer);
    fileLayout->setContentsMargins(10, 5, 10, 5);
    fileLayout->setSpacing(10);

    // 文件图标
    QLabel* fileIcon = new QLabel(this);
    QPixmap iconPixmap(":/icons/file.png");
    fileIcon->setPixmap(iconPixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    fileLayout->addWidget(fileIcon);

    // 信息区
    QWidget* infoContainer = new QWidget(this);
    QVBoxLayout* infoLayout = new QVBoxLayout(infoContainer);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(5);

    // 发送者
    QLabel* senderLabel = new QLabel(this);
    senderLabel->setText("发送者: " + sender);
    senderLabel->setStyleSheet("color: #666; font-size: 12px;");
    infoLayout->addWidget(senderLabel);

    // 文件名
    QLabel* fileNameLabel = new QLabel(this);
    fileNameLabel->setText(filename);
    fileNameLabel->setStyleSheet("color: #333; font-weight: bold;");
    infoLayout->addWidget(fileNameLabel);

    // 文件大小
    QLabel* fileSizeLabel = new QLabel(this);
    QString sizeStr;
    if (filesize < 1024)
        sizeStr = QString::number(filesize) + " B";
    else if (filesize < 1024 * 1024)
        sizeStr = QString::number(filesize / 1024.0, 'f', 2) + " KB";
    else
        sizeStr = QString::number(filesize / (1024.0 * 1024.0), 'f', 2) + " MB";
    fileSizeLabel->setText("大小: " + sizeStr);
    fileSizeLabel->setStyleSheet("color: #666; font-size: 12px;");
    infoLayout->addWidget(fileSizeLabel);

    fileLayout->addWidget(infoContainer);

    // 下载按钮
    QPushButton* downloadBtn = new QPushButton(this);
    downloadBtn->setText("下载");
    downloadBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    border: none;"
        "    padding: 5px 15px;"
        "    border-radius: 3px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #45a049;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #3d8b40;"
        "}"
    );
    fileLayout->addWidget(downloadBtn);

    fileContainer->setStyleSheet(
        "QWidget {"
        "    background-color: #f5f5f5;"
        "    border-radius: 5px;"
        "    border: 1px solid #ddd;"
        "}"
    );

    // 添加到聊天框
    QListWidgetItem* item = new QListWidgetItem();
    item->setSizeHint(fileContainer->sizeHint());
    ui->chatWindow_lw->addItem(item);
    ui->chatWindow_lw->setItemWidget(item, fileContainer);
    ui->chatWindow_lw->scrollToBottom();

    // 下载按钮事件（此处仅作演示，实际下载逻辑需完善）
    connect(downloadBtn, &QPushButton::clicked, [filename]() {
        QMessageBox::information(nullptr, "下载", "下载功能待实现: " + filename);
    });
}

void Widget::onFileDownloadClicked()
{
    // 找到按钮对应的文件名
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    for (int i = 0; i < ui->fileList_lw->count(); ++i) {
        QListWidgetItem* item = ui->fileList_lw->item(i);
        QWidget* widget = ui->fileList_lw->itemWidget(item);
        if (widget && widget->findChild<QPushButton*>("下载") == btn) {
            QString filename = item->data(Qt::UserRole).toString();
            // 选择保存路径
            QString savePath = QFileDialog::getSaveFileName(this, "保存文件", filename);
            if (savePath.isEmpty()) return;
            m_fileTransfer->downloadFile(savePath, "localhost", 8888);
            m_downloadedFiles[filename] = savePath;
            break;
        }
    }
}

void Widget::onFileOpenClicked()
{
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    for (int i = 0; i < ui->fileList_lw->count(); ++i) {
        QListWidgetItem* item = ui->fileList_lw->item(i);
        QWidget* widget = ui->fileList_lw->itemWidget(item);
        if (widget && widget->findChild<QPushButton*>("打开") == btn) {
            QString filename = item->data(Qt::UserRole).toString();
            QString localPath = m_downloadedFiles.value(filename);
            if (!localPath.isEmpty() && QFileInfo::exists(localPath)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
            } else {
                // 未下载则先下载再打开
                QString savePath = QFileDialog::getSaveFileName(this, "保存并打开文件", filename);
                if (savePath.isEmpty()) return;
                m_fileTransfer->downloadFile(savePath, "localhost", 8888);
                m_downloadedFiles[filename] = savePath;
                // 下载完成后自动打开
                connect(m_fileTransfer, &FileTransfer::downloadFinished, [savePath]() {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
                });
            }
            break;
        }
    }
}









