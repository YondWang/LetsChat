#pragma once
#include "CYondLog.h"
#include "Packet.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <map>
#include <string>
#include <vector>

class CYondHandleEvent {
public:
	CYondHandleEvent() : m_epollFd(-1) {}
	~CYondHandleEvent() {
		if (m_epollFd != -1) {
			close(m_epollFd);
		}
	}

	bool Initialize() {
		m_epollFd = epoll_create1(0);
		if (m_epollFd == -1) {
			LOG_ERROR(YOND_ERR_EPOLL_CREATE, "Failed to create epoll instance");
			return false;
		}
		return true;
	}

	bool addNew(int sockfd, const std::string& clientAddr) {
		struct epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.fd = sockfd;

		if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
			LOG_ERROR(YOND_ERR_EPOLL_CTL, "Failed to add socket to epoll");
			return false;
		}

		m_clients[sockfd] = clientAddr;
		LOG_INFO("New client connected: " + clientAddr);

		// 发送欢迎消息
		Packet welcomeMsg = Packet::createSystemMessage("Welcome to the chat server!");
		sendPacket(sockfd, welcomeMsg);
		return true;
	}

	void HandleEvent(int sockfd) {
		char buffer[Packet::MAX_DATA_SIZE];
		ssize_t bytesRead = recv(sockfd, buffer, sizeof(buffer), 0);

		if (bytesRead <= 0) {
			if (bytesRead == 0) {
				LOG_INFO("Client disconnected: " + m_clients[sockfd]);
			} else {
				LOG_ERROR(YOND_ERR_SOCKET_RECV, "Error receiving data from client: " + m_clients[sockfd]);
			}
			removeClient(sockfd);
			return;
		}

		Packet packet;
		if (!packet.parse(buffer, bytesRead)) {
			LOG_ERROR(YOND_ERR_PACKET_PARSE, "Failed to parse packet from client: " + m_clients[sockfd]);
			return;
		}

		ProcessMessage(sockfd, packet);
	}

private:
	void ProcessMessage(int sockfd, const Packet& packet) {
		switch (packet.getType()) {
			case Packet::Type::ChatMessage: {
				std::string sender, content;
				packet.getChatMessage(sender, content);
				broadcastMessage(sender, content);
				break;
			}
			case Packet::Type::FileStart: {
				std::string fileName;
				uint32_t fileSize;
				packet.getFileStart(fileName, fileSize);
				handleFileStart(sockfd, fileName, fileSize);
				break;
			}
			case Packet::Type::FileData: {
				const char* data;
				uint32_t size;
				packet.getFileData(data, size);
				handleFileData(sockfd, data, size);
				break;
			}
			case Packet::Type::FileEnd:
				handleFileEnd(sockfd);
				break;
			case Packet::Type::SystemMessage: {
				std::string content;
				packet.getSystemMessage(content);
				broadcastSystemMessage(content);
				break;
			}
		}
	}

	void broadcastMessage(const std::string& sender, const std::string& content) {
		Packet packet = Packet::createChatMessage(sender, content);
		for (const auto& client : m_clients) {
			sendPacket(client.first, packet);
		}
	}

	void broadcastSystemMessage(const std::string& content) {
		Packet packet = Packet::createSystemMessage(content);
		for (const auto& client : m_clients) {
			sendPacket(client.first, packet);
		}
	}

	void handleFileStart(int sockfd, const std::string& fileName, uint32_t fileSize) {
		// 实现文件传输开始的处理逻辑
		LOG_INFO("File transfer started: " + fileName + " (" + std::to_string(fileSize) + " bytes)");
	}

	void handleFileData(int sockfd, const char* data, uint32_t size) {
		// 实现文件数据传输的处理逻辑
		LOG_INFO("Received file data: " + std::to_string(size) + " bytes");
	}

	void handleFileEnd(int sockfd) {
		// 实现文件传输结束的处理逻辑
		LOG_INFO("File transfer completed");
	}

	void sendPacket(int sockfd, const Packet& packet) {
		const PacketHeader* header = packet.getHeader();
		const char* data = packet.getData();
		
		// 发送头部
		if (send(sockfd, header, packet.getHeaderSize(), 0) == -1) {
			LOG_ERROR(YOND_ERR_SOCKET_SEND, "Failed to send packet header");
			return;
		}

		// 发送数据
		if (packet.getDataSize() > 0 && send(sockfd, data, packet.getDataSize(), 0) == -1) {
			LOG_ERROR(YOND_ERR_SOCKET_SEND, "Failed to send packet data");
		}
	}

	void removeClient(int sockfd) {
		if (m_clients.find(sockfd) != m_clients.end()) {
			m_clients.erase(sockfd);
			close(sockfd);
		}
	}

	int m_epollFd;
	std::map<int, std::string> m_clients;
};

