#pragma once
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "CYondThreadPool.h"
#include "CYondLog.h"
#include <arpa/inet.h>
#include "CYondPack.h"
#include <iostream>
#include <unordered_map>

// 文件传输状态结构
struct FileTransferState {
    std::string fileName;
    size_t totalSize;
    size_t receivedSize;
    std::queue<std::string> receivedPackets;  // 数据包队列
    std::unique_ptr<std::mutex> mutex;
    std::unique_ptr<std::condition_variable> cv;
    bool isComplete;

    FileTransferState() 
        : totalSize(0)
        , receivedSize(0)
        , mutex(std::make_unique<std::mutex>())
        , cv(std::make_unique<std::condition_variable>())
        , isComplete(false) {}

    FileTransferState(
        const std::string& name,
        size_t total,
        size_t received,
        std::queue<std::string>&& packets,
        std::unique_ptr<std::mutex>&& mtx,
        std::unique_ptr<std::condition_variable>&& cv_ptr,
        bool complete
    ) : fileName(name)
        , totalSize(total)
        , receivedSize(received)
        , receivedPackets(std::move(packets))
        , mutex(std::move(mtx))
        , cv(std::move(cv_ptr))
        , isComplete(complete) {}

    FileTransferState(const FileTransferState&) = delete;
    FileTransferState& operator=(const FileTransferState&) = delete;
    FileTransferState(FileTransferState&&) = default;
    FileTransferState& operator=(FileTransferState&&) = default;
};

class CYondHandleEvent
{
public:
    CYondHandleEvent() : m_threadPool(), m_fileTransferThreadPool(2) {
        LOG_INFO("Thread pools initialized");
    }

	int addNew(epoll_event* epEvt, int epollFd) {
		struct sockaddr_in clientAddr;
		socklen_t clientLen = sizeof(clientAddr);
		int clientFd = accept(epEvt->data.fd, (struct sockaddr*)&clientAddr, &clientLen);
		//LOG_INFO("accept a new client!");
		if (clientFd < 0) {
			return LOG_ERROR(YOND_ERR_SOCKET_ACCEPT, "Failed to accept new connection");
		}

		// 将新客户端添加到epoll
		struct epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.fd = clientFd;
		if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &ev) < 0) {
			close(clientFd);
			return LOG_ERROR(YOND_ERR_EPOLL_CTL, "Failed to add client to epoll");
		}

		return 0;
	}

	int HandleEvent(epoll_event* events) {
		char buffer[8192];
		size_t n = recv(events->data.fd, buffer, sizeof(buffer), 0);

		if (n <= 0) {
			// 客户端断开连接
			LOG_INFO("Client disconnected: " + m_clientFdToIp[events->data.fd]);
			BroadCastToAll(events->data.fd, m_clientFdToIp[events->data.fd], YDisCon);
			close(events->data.fd);
			m_clientFdToIp.erase(events->data.fd);
			m_recvBuffer.erase(events->data.fd);
			return 0;
		}

		// 粘包处理：追加到缓冲区并循环提取完整包
		m_recvBuffer[events->data.fd].append(buffer, n);
		ExtractAndProcessPackets(events->data.fd);

		return 0;
	}

	void HandleFileStart(int clientFd, const CYondPack& msg) {
		// 解析文件名和总大小
		std::string fileInfo = msg.m_strData;
		size_t pos = fileInfo.find('|');
		if (pos == std::string::npos) {
			LOG_ERROR(YOND_ERR_RECV_PACKET, "Invalid file start message format");
			return;
		}

		std::string fileName = fileInfo.substr(0, pos);
		size_t totalSize = std::stoull(fileInfo.substr(pos + 1));

		// 创建新的文件传输状态
		std::lock_guard<std::mutex> lock(m_fileTransfersMutex);
		m_fileTransfers[clientFd] = FileTransferState{
			fileName,
			totalSize,
			0,
			std::queue<std::string>(),
			std::unique_ptr<std::mutex>(new std::mutex()),
			std::unique_ptr<std::condition_variable>(new std::condition_variable()),
			false
		};

		// 发送确认消息
		CYondPack ackMsg(YFileAck, msg.m_sUser, "START", 5);
		if (send(clientFd, ackMsg.Data().data(), ackMsg.Size(), 0) <= 0) {
			LOG_ERROR(YOND_ERR_SOCKET_SEND, "err to send file ack!");
		}
	}

	void HandleFileData(int clientFd, const CYondPack& msg);

	void HandleFileEnd(int clientFd, const CYondPack& msg);

	void HandleFileAck(int clientFd, const CYondPack& msg) {
		// 处理文件传输确认消息
		LOG_INFO("Received file transfer acknowledgment from client " + std::to_string(clientFd));
	}

	void BroadCastToAll(int senderFd, const std::string& message, YondCmd cmd = YMsg, int userId = -1) {
		for (auto& client : m_clientFdToIp) {
			if (client.first == senderFd) continue; // 不发给自己

			unsigned short realUserId = (userId == -1) ? (unsigned short)(senderFd & 0xFFFF) : (unsigned short)(userId & 0xFFFF);

			CYondPack broadcastMsg(cmd, realUserId, message.c_str(), message.size());
			broadcastMsg.PrintMessage();

			const char* data = broadcastMsg.Data().data();
			size_t size = broadcastMsg.Size();

			if (send(client.first, data, size, 0) < 0) {
				LOG_ERROR(YOND_ERR_SOCKET_SEND, "Failed to broadcast message to client " + client.second);
			}
		}
	}

	const char* StrToMsg(std::string* Str) {
		return 0;
	}

	std::string* MsgToStr(CYondPack* Pack) {
		return 0;
	}

	CYondThreadPool m_threadPool;
	CYondThreadPool m_fileTransferThreadPool;
	std::map<int, std::string> m_clientFdToIp;
	std::map<int, FileTransferState> m_fileTransfers;
	std::mutex m_fileTransfersMutex;
	std::unordered_map<int, std::string> m_recvBuffer;
	//void ProcessMessage(int clientFd, const std::string& data);
	void ProcessMessage(int clientFd, const CYondPack& msg);
	void ExtractAndProcessPackets(int clientFd);

	// 文件传输说明：
	// 1. 客户端发送YFileStart，内容为 文件名|文件总字节数
	// 2. 客户端分包发送YFileData，内容为文件二进制数据
	// 3. 客户端发送YFileEnd，通知服务端写入文件
	// 4. 服务端收到YFileStart后，创建FileTransferState，收到YFileData时入队，收到YFileEnd后在独立线程中写入文件
	// 5. 所有文件传输相关操作均加锁，保证线程安全
};

