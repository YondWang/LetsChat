#pragma once
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <map>
#include "CYondThreadPool.h"
#include "CYondLog.h"
#include <arpa/inet.h>
#include "CYondPack.h"
#include <iostream>

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
			BroadCastToAll(events->data.fd, "", YDisCon);
			close(events->data.fd);
			m_clientFdToIp.erase(events->data.fd);
			return 0;
		}

		// 将消息处理任务提交到线程池
		m_threadPool.Enqueue([this, fd = events->data.fd, data = std::string(buffer, n)]() {
			ProcessMessage(fd, data);
		});

		return 0;
	}

private:
	void ProcessMessage(int clientFd, const std::string& data) {
		CYondPack msg;
		size_t size = data.size();
		const unsigned char* pData = (const unsigned char*)(data.c_str());
		msg = CYondPack(pData, size);

		if (size == 0) {
			LOG_ERROR(YOND_ERR_RECV_PACKET, "Invalid message format");
			return;
		}

		switch (msg.m_sCmd) {
		case YConnect:
			// 处理连接请求
			// 记录新客户端
			m_clientFdToIp[clientFd] = msg.m_strData;
			LOG_INFO("Client " + m_clientFdToIp[clientFd] + " connected" + " broad login msg!");
			BroadCastToAll(clientFd, msg.m_strData, YConnect);
			break;

		case YMsg:
			// 广播消息给所有客户端
			if (!msg.m_strData.empty()) {
				LOG_INFO("Broadcasting message from " + m_clientFdToIp[clientFd] + ": " + msg.m_strData);
				BroadCastToAll(clientFd, msg.m_strData);
			}
			break;

		case YFile:
			// 处理文件传输请求
			if (!msg.m_strData.empty()) {
				LOG_INFO("File transfer request from " + m_clientFdToIp[clientFd] + ": " + msg.m_strData);
				// TODO: 实现文件传输逻辑
			}
			break;

		case YRecv:
			// 处理文件接收请求
			if (!msg.m_strData.empty()) {
				LOG_INFO("File receive request from " + m_clientFdToIp[clientFd] + ": " + msg.m_strData);
				// TODO: 实现文件接收逻辑
			}
			break;
		
		default:
			LOG_WARNING("Unknown message type: " + std::to_string(msg.m_sCmd));
		}
	}

	void BroadCastToAll(int senderFd, const std::string& message , YondCmd cmd = YMsg) {
		CYondPack broadcastMsg(cmd, senderFd, message.c_str(), message.size());
		const char* data = broadcastMsg.Data().data();
		size_t size = broadcastMsg.Size();

		LOG_INFO("Broad raw data:");
		for (size_t i = 0; i < size; i++) {
			printf("%02X ", (unsigned char)data[i]);
		}
		printf("\r\n");

		for (const auto& client : m_clientFdToIp) {
			if (client.first != senderFd) {  // 不发送给发送者
				if (send(client.first, data, size, 0) < 0) {
					LOG_ERROR(YOND_ERR_SOCKET_SEND, "Failed to broadcast message to client " + client.second);
				}
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
	std::map<int, std::string> m_clientFdToIp; // 客户端fd到IP地址的映射
};

