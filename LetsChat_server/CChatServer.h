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


#define PORT 2903
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
		if(m_nSockFd != -1) 
			return LOG_ERROR(YOND_ERR_SOCKET_CREATE, "Socket already initialized");

		m_nSockFd = socket(AF_INET, SOCK_STREAM, 0);
		memset(&m_addr, 0, sizeof(m_addr));
		m_addr.sin_family = AF_INET;
		m_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		m_addr.sin_port = htons(PORT);

		if (bind(m_nSockFd, (sockaddr*)&m_addr, sizeof(m_addr))) {
			close(m_nSockFd);
			m_nSockFd = -1;
			return LOG_ERROR(YOND_ERR_SOCKET_BIND, "Failed to bind socket");
		}

		if (listen(m_nSockFd, 5)) {
			close(m_nSockFd);
			m_nSockFd = -1;
			return LOG_ERROR(YOND_ERR_SOCKET_LISTEN, "Failed to listen on socket");
		}

		if (m_nSockFd < 0) {
			return LOG_ERROR(YOND_ERR_SOCKET_CREATE, "Failed to initialize socket");
		}

		LOG_INFO("Socket initialized successfully");
		return 0;
	}

	CChatServer() : m_nSockFd(-1), m_nEpollFd(-1), m_bStop(true) {
		LOG_INFO("Chat server instance created");
	}

private:
	int m_nPort;
	int m_nSockFd;
	sockaddr_in m_addr;
	int m_nEpollFd;
	CYondHandleEvent m_handleEvent;
	static CChatServer* m_instance;
	bool m_bStop;
};

