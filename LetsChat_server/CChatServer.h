#pragma once
#include <sys/socket.h>
#include "CYondErr.h"
#include <netinet/in.h>
#include <sys/stat.h>
#include <cstdio>
#include <unistd.h>
#include <string.h>
#include "CYondLog.h"
#include "CYondHandleEvent.h"
#include "CYondThreadPool.h"
#include <error.h>


#define MSG_PORT 2903
#define FILE_PORT 2904
#define MAX_EVENTS 100

class CChatServer
{
public:
	static CChatServer* GetInstance() {
		if (m_instance == nullptr) {
			m_instance = new CChatServer();
		}
		return m_instance;
	}

	int EpollDo (epoll_event* alevt);
	int StartService();
	int StopService();

	int InitSocket() {
		// 消息端口
		m_nSockFdMsg = socket(AF_INET, SOCK_STREAM, 0);
		memset(&m_addrMsg, 0, sizeof(m_addrMsg));
		m_addrMsg.sin_family = AF_INET;
		m_addrMsg.sin_addr.s_addr = htonl(INADDR_ANY);
		m_addrMsg.sin_port = htons(MSG_PORT);
		if (bind(m_nSockFdMsg, (sockaddr*)&m_addrMsg, sizeof(m_addrMsg))) {
			close(m_nSockFdMsg);
			m_nSockFdMsg = -1;
			return LOG_ERROR(YOND_ERR_SOCKET_BIND, "Failed to bind msg socket");
		}
		if (listen(m_nSockFdMsg, 5)) {
			close(m_nSockFdMsg);
			m_nSockFdMsg = -1;
			return LOG_ERROR(YOND_ERR_SOCKET_LISTEN, "Failed to listen on msg socket");
		}
		// 文件端口
		m_nSockFdFile = socket(AF_INET, SOCK_STREAM, 0);
		memset(&m_addrFile, 0, sizeof(m_addrFile));
		m_addrFile.sin_family = AF_INET;
		m_addrFile.sin_addr.s_addr = htonl(INADDR_ANY);
		m_addrFile.sin_port = htons(FILE_PORT);
		if (bind(m_nSockFdFile, (sockaddr*)&m_addrFile, sizeof(m_addrFile))) {
			close(m_nSockFdFile);
			m_nSockFdFile = -1;
			return LOG_ERROR(YOND_ERR_SOCKET_BIND, "Failed to bind file socket");
		}
		if (listen(m_nSockFdFile, 5)) {
			close(m_nSockFdFile);
			m_nSockFdFile = -1;
			return LOG_ERROR(YOND_ERR_SOCKET_LISTEN, "Failed to listen on file socket");
		}
		LOG_INFO("Sockets initialized successfully");
		return 0;
	}

	CChatServer() : m_nSockFdMsg(-1), m_nSockFdFile(-1), m_nEpollFd(-1), m_bStop(true) {
		LOG_INFO("Chat server instance created");
	}

private:
	int m_nSockFdMsg; // 消息socket
	int m_nSockFdFile; // 文件socket
	sockaddr_in m_addrMsg;
	sockaddr_in m_addrFile;
	int m_nEpollFd;
	CYondHandleEvent m_handleEvent;
	static CChatServer* m_instance;
	bool m_bStop;
};

