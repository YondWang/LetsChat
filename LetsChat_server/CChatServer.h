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
#include <error.h>
#include <memory>

#define PORT 2903
#define MAX_EVENTS 100

class CChatServer
{
public:
	static CChatServer* GetInstance() {
		static CChatServer instance;
		return &instance;
	}

	int StartService() {
		int err = 0;
		err = InitSocket();

		m_nEpollFd = epoll_create1(0);
		if (m_nEpollFd < 0) {
			return LOG_ERROR(YOND_ERR_EPOLL_CREATE, "Failed to create epoll instance");
		}

		// 初始化事件处理器
		m_handleEvent = std::make_unique<CYondHandleEvent>(m_nEpollFd);

		struct epoll_event event, all_events[MAX_EVENTS];
		event.events = EPOLLIN;
		event.data.fd = m_nSockFd;
		if (epoll_ctl(m_nEpollFd, EPOLL_CTL_ADD, m_nSockFd, &event) < 0) {
			return LOG_ERROR(YOND_ERR_EPOLL_CTL, "Failed to add socket to epoll");
		}

		LOG_INFO("Server started, waiting for connections...");

		while (1) {
			int eventMnt = epoll_wait(m_nEpollFd, all_events, MAX_EVENTS, 1000);
			if (eventMnt == -1) {
				return LOG_ERROR(YOND_ERR_EPOLL_WAIT, "Failed to wait for epoll events");
			}
			for (int i = 0; i < eventMnt; i++) {
				if (all_events[i].data.fd == m_nSockFd) {
					err = m_handleEvent->addNew(&all_events[i]);
				}
				else {
					err = m_handleEvent->HandleEvent(&all_events[i]);
				}
			}
		}

		close(m_nSockFd);
		close(m_nEpollFd);
		return err;
	}

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

	CChatServer() : m_nSockFd(-1), m_nEpollFd(-1) {
		LOG_INFO("Chat server instance created");
	}

private:
	int m_nSockFd;
	int m_nEpollFd;
	std::unique_ptr<CYondHandleEvent> m_handleEvent;
	sockaddr_in m_addr;
};

