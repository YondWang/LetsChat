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

// 消息包结构
struct ChatMessage {
	int type;           // 消息类型
	std::string sender; // 发送者
	std::string content;// 消息内容
	std::string target; // 目标接收者
};

class CYondHandleEvent
{
public:
	CYondHandleEvent() : m_threadPool(4) {
		LOG_INFO("Thread pool initialized with 4 worker threads");
	}

	int addNew(epoll_event* epEvt) {
		struct sockaddr_in clientAddr;
		socklen_t clientLen = sizeof(clientAddr);
		int clientFd = accept(epEvt->data.fd, (struct sockaddr*)&clientAddr, &clientLen);
		
		if (clientFd < 0) {
			return LOG_ERROR(YOND_ERR_SOCKET_ACCEPT, "Failed to accept new connection");
		}

		// 将新客户端添加到epoll
		struct epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.fd = clientFd;
		if (epoll_ctl(epEvt->data.fd, EPOLL_CTL_ADD, clientFd, &ev) < 0) {
			close(clientFd);
			return LOG_ERROR(YOND_ERR_EPOLL_CTL, "Failed to add client to epoll");
		}

		// 记录新客户端
		m_clients[clientFd] = std::string(inet_ntoa(clientAddr.sin_addr));
		LOG_INFO("New client connected: " + m_clients[clientFd]);
		
		return 0;
	}

	int HandleEvent(epoll_event* events) {
		char buffer[1024];
		size_t n = recv(events->data.fd, buffer, sizeof(buffer), 0);
		
		if (n <= 0) {
			// 客户端断开连接
			LOG_INFO("Client disconnected: " + m_clients[events->data.fd]);
			close(events->data.fd);
			m_clients.erase(events->data.fd);
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
		// 这里实现消息解析和处理逻辑
		ChatMessage msg;
		// TODO: 解析data到msg结构
		
		switch (msg.type) {
			case 1: // 私聊消息
				HandlePrivateMessage(msg);
				break;
			case 2: // 群发消息
				HandleBroadcastMessage(msg);
				break;
			default:
				LOG_WARNING("Unknown message type: " + std::to_string(msg.type));
		}
	}

	void HandlePrivateMessage(const ChatMessage& msg) {
		LOG_INFO("Private message from " + msg.sender + " to " + msg.target);
		// 实现私聊消息处理逻辑
		// TODO: 根据msg.target找到目标客户端并发送消息
	}

	void HandleBroadcastMessage(const ChatMessage& msg) {
		LOG_INFO("Broadcast message from " + msg.sender);
		// 实现群发消息处理逻辑
		// TODO: 向所有客户端广播消息
	}

	CYondThreadPool m_threadPool;
	std::map<int, std::string> m_clients; // 客户端fd到IP地址的映射
};

