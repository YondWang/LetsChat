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
	std::queue<std::pair<size_t, std::string>> receivedPackets;  // 序列号和数据包
	std::unique_ptr<std::mutex> mutex;
	std::unique_ptr<std::condition_variable> cv;
	bool isComplete;
	std::map<size_t, bool> receivedSequences;  // 记录已接收的序列号
	size_t expectedSequence;  // 期望的下一个序列号

	FileTransferState() 
		: totalSize(0)
		, receivedSize(0)
		, mutex(std::make_unique<std::mutex>())
		, cv(std::make_unique<std::condition_variable>())
		, isComplete(false)
		, expectedSequence(0) {}

	// 添加接受初始化列表的构造函数
	FileTransferState(
		const std::string& name,
		size_t total,
		size_t received,
		std::queue<std::pair<size_t, std::string>>&& packets,
		std::unique_ptr<std::mutex>&& mtx,
		std::unique_ptr<std::condition_variable>&& cv_ptr,
		bool complete
	) : fileName(name)
		, totalSize(total)
		, receivedSize(received)
		, receivedPackets(std::move(packets))
		, mutex(std::move(mtx))
		, cv(std::move(cv_ptr))
		, isComplete(complete)
		, expectedSequence(0) {}

	// 禁用拷贝
	FileTransferState(const FileTransferState&) = delete;
	FileTransferState& operator=(const FileTransferState&) = delete;

	// 启用移动
	FileTransferState(FileTransferState&&) = default;
	FileTransferState& operator=(FileTransferState&&) = default;
};

class CYondHandleEvent
{
public:
	CYondHandleEvent() : m_threadPool() {
		LOG_INFO("Thread pool initialized with 4 worker threads");
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
		char buffer[2048];
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

private:
	void ProcessMessage(int clientFd, const std::string& data) {
		CYondPack msg;
		size_t size = data.size();
		const char* pData = (const char*)(data.c_str());
		msg = CYondPack(pData, size);

		if (size == 0) {
			LOG_ERROR(YOND_ERR_RECV_PACKET, "Invalid message format");
			return;
		}
		LOG_INFO("receive message type:" + std::to_string(msg.m_sCmd));
		if (msg.m_sCmd == YFileStart || msg.m_sCmd == YFileData || msg.m_sCmd == YFileEnd || msg.m_sCmd == YFileAck) {
			// 提取序列号
			size_t sequence = 0;
			if (msg.m_strData.size() >= 2) {  // 确保至少有序列号数据
				memcpy(&sequence, msg.m_strData.c_str(), 2);
				sequence = ntohs(sequence);  // 转换网络字节序
			}

			// 验证序列号
			if (!validateSequence(clientFd, sequence)) {
				// 序列号无效，请求重传
				auto it = m_clientSequences.find(clientFd);
				if (it != m_clientSequences.end()) {
					requestMissingPackets(clientFd, it->second, sequence);
				}
				// 缓存乱序消息
				handleOutOfOrderPacket(clientFd, msg, sequence);
				return;
			}

			// 更新序列号
			updateClientSequence(clientFd, sequence);

			
		}
		// 处理消息内容（去掉序列号部分）
		msg.m_strData = msg.m_strData.substr(2);

		switch (msg.m_sCmd) {
		case YConnect: {
			// 记录新客户端
			m_clientFdToIp[clientFd] = msg.m_strData;
			// 初始化序列号和消息缓存
			m_clientSequences[clientFd] = 0;
			m_messageCache[clientFd] = MessageCache();
			LOG_INFO("Client " + m_clientFdToIp[clientFd] + " connected" + " broad login msg!");
			// 发送所有在线用户列表
			std::string userList;
			for (const auto& kv : m_clientFdToIp) {
				unsigned short uid = (unsigned short)(kv.first & 0xFFFF);
				userList += std::to_string(uid) + "|" + kv.second + ";";
			}
			if (!userList.empty()) userList.pop_back(); // 去掉最后一个分号
			// 构造数据区：2字节序列号+userList
			std::string dataWithSeq(2, 0);
			*(unsigned short*)dataWithSeq.data() = htons(m_clientSequences[clientFd]);
			dataWithSeq += userList;
			CYondPack userListMsg(YUserList, clientFd, dataWithSeq.c_str(), dataWithSeq.size());
			send(clientFd, userListMsg.Data().data(), userListMsg.Size(), 0);
			m_clientSequences[clientFd]++;
			// 广播新用户上线
			BroadCastToAll(clientFd, msg.m_strData, YConnect, clientFd);
			break;
		}

		case YMsg:
			// 广播消息给所有客户端
			if (!msg.m_strData.empty()) {
				LOG_INFO("Broadcasting message from " + m_clientFdToIp[clientFd] + ": " + msg.m_strData);
				BroadCastToAll(clientFd, msg.m_strData, YMsg, clientFd);
			}
			break;

		case YFileStart:
			HandleFileStart(clientFd, msg);
			break;

		case YFileData:
			HandleFileData(clientFd, msg);
			break;

		case YFileEnd:
			HandleFileEnd(clientFd, msg);
			break;

		case YFileAck:
			HandleFileAck(clientFd, msg);
			break;

		default:
			LOG_WARNING("Unknown message type: " + std::to_string(msg.m_sCmd));
		}
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
			std::queue<std::pair<size_t, std::string>>(),
			std::unique_ptr<std::mutex>(new std::mutex()),
			std::unique_ptr<std::condition_variable>(new std::condition_variable()),
			false
		};

		// 发送确认消息
		CYondPack ackMsg(YFileAck, msg.m_sUser, "START", 5);
		send(clientFd, ackMsg.Data().data(), ackMsg.Size(), 0);
	}

	void HandleFileData(int clientFd, const CYondPack& msg) {
		std::lock_guard<std::mutex> lock(m_fileTransfersMutex);
		auto it = m_fileTransfers.find(clientFd);
		if (it == m_fileTransfers.end()) {
			LOG_ERROR(YOND_ERR_RECV_PACKET, "No active file transfer for client");
			return;
		}

		FileTransferState& state = it->second;
		std::lock_guard<std::mutex> stateLock(*state.mutex);

		// 解析序列号和数据
		std::string data = msg.m_strData;
		size_t pos = data.find('|');
		if (pos == std::string::npos) {
			LOG_ERROR(YOND_ERR_RECV_PACKET, "Invalid file data message format");
			return;
		}

		size_t seqNum = std::stoull(data.substr(0, pos));
		std::string packetData = data.substr(pos + 1);

		// 将数据包添加到队列
		state.receivedPackets.push({seqNum, packetData});
		state.receivedSize += packetData.size();

		// 发送确认消息
		CYondPack ackMsg(YFileAck, msg.m_sUser, std::to_string(seqNum).c_str(), std::to_string(seqNum).size());
		send(clientFd, ackMsg.Data().data(), ackMsg.Size(), 0);

		// 检查是否接收完成
		if (state.receivedSize >= state.totalSize) {
			state.isComplete = true;
			state.cv->notify_all();
		}
	}

	void HandleFileEnd(int clientFd, const CYondPack& msg) {
		std::lock_guard<std::mutex> lock(m_fileTransfersMutex);
		auto it = m_fileTransfers.find(clientFd);
		if (it == m_fileTransfers.end()) {
			LOG_ERROR(YOND_ERR_RECV_PACKET, "No active file transfer for client");
			return;
		}

		FileTransferState& state = it->second;
		std::unique_lock<std::mutex> stateLock(*state.mutex);

		// 等待所有数据包接收完成
		state.cv->wait(stateLock, [&state] { return state.isComplete; });

		// 按顺序写入文件
		std::string filePath = "received_files/" + state.fileName;
		std::ofstream file(filePath, std::ios::binary);
		if (!file.is_open()) {
			LOG_ERROR(YOND_ERR_RECV_PACKET, "Failed to open file for writing: " + filePath);
			return;
		}

		while (!state.receivedPackets.empty()) {
			auto& packet = state.receivedPackets.front();
			file.write(packet.second.c_str(), packet.second.size());
			state.receivedPackets.pop();
		}

		file.close();

		// 发送完成确认
		CYondPack ackMsg(YFileAck, msg.m_sUser, "END", 3);
		send(clientFd, ackMsg.Data().data(), ackMsg.Size(), 0);

		// 清理传输状态
		m_fileTransfers.erase(it);
	}

	void HandleFileAck(int clientFd, const CYondPack& msg) {
		// 处理文件传输确认消息
		LOG_INFO("Received file transfer acknowledgment from client " + std::to_string(clientFd));
	}

	void BroadCastToAll(int senderFd, const std::string& message, YondCmd cmd = YMsg, int userId = -1) {
		for (auto& client : m_clientFdToIp) {
			if (client.first == senderFd) continue; // 不发给自己

			unsigned short realUserId = (userId == -1) ? (unsigned short)(senderFd & 0xFFFF) : (unsigned short)(userId & 0xFFFF);

			// 获取该客户端的当前序列号
			size_t seq = m_clientSequences[client.first];
			std::string dataWithSeq(2, 0);
			*(unsigned short*)dataWithSeq.data() = htons(seq);
			dataWithSeq += message;

			CYondPack broadcastMsg(cmd, realUserId, dataWithSeq.c_str(), dataWithSeq.size());
			broadcastMsg.PrintMessage();

			const char* data = broadcastMsg.Data().data();
			size_t size = broadcastMsg.Size();

			if (send(client.first, data, size, 0) < 0) {
				LOG_ERROR(YOND_ERR_SOCKET_SEND, "Failed to broadcast message to client " + client.second);
			}

			// 广播后自增该客户端的序列号
			m_clientSequences[client.first] = seq + 1;
		}
	}

	/*void BroadToAllDrc(const char* data, size_t size, int senderFd) {
		for (const auto& client : m_clientFdToIp) {
			if (client.first != senderFd) {
				if (send(client.first, data, size, 0) < 0) {
					LOG_ERROR(YOND_ERR_SOCKET_SEND, "Failed to broadcast message to client " + client.second);
				}
			}
		}
	}*/

	const char* StrToMsg(std::string* Str) {
		return 0;
	}

	std::string* MsgToStr(CYondPack* Pack) {
		return 0;
	}

	CYondThreadPool m_threadPool;
	std::map<int, std::string> m_clientFdToIp;  // 客户端fd到IP地址的映射
	std::map<int, FileTransferState> m_fileTransfers;  // 文件传输状态映射
	std::mutex m_fileTransfersMutex;  // 文件传输状态互斥锁

	// 序列号相关的成员变量
	std::map<int, size_t> m_clientSequences;  // 每个客户端的当前序列号
	std::mutex m_sequenceMutex;  // 序列号访问互斥锁
	
	// 序列号处理相关的函数
	bool validateSequence(int clientFd, size_t sequence);
	void updateClientSequence(int clientFd, size_t sequence);
	void handleOutOfOrderPacket(int clientFd, const CYondPack& msg, size_t sequence);
	void requestMissingPackets(int clientFd, size_t expectedSeq, size_t receivedSeq);

	void ProcessMessage(int clientFd, const CYondPack& msg);

	// 客户端消息缓存
	struct MessageCache {
		std::map<size_t, CYondPack> messages;      // 序列号 -> 消息映射
		size_t lastProcessedSeq;                   // 最后处理的序列号
		std::unique_ptr<std::mutex> mutex;         // 互斥锁

		MessageCache() 
			: lastProcessedSeq(0)
			, mutex(std::make_unique<std::mutex>()) {}

		// 禁用拷贝
		MessageCache(const MessageCache&) = delete;
		MessageCache& operator=(const MessageCache&) = delete;

		// 启用移动
		MessageCache(MessageCache&&) = default;
		MessageCache& operator=(MessageCache&&) = default;
	};
	std::unordered_map<int, MessageCache> m_messageCache;  // 客户端FD -> 消息缓存

	std::unordered_map<int, std::string> m_recvBuffer; // 每个客户端的接收缓冲区

	void ExtractAndProcessPackets(int clientFd);
};

