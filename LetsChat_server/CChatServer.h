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


#define PORT 2904
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

		m_nEpollFd = epoll_create(0);
		if (m_nEpollFd < 0) {
			err = CYondLog::log(CYondLog::LOG_LEVEL_ERROR, __FILE__, __LINE__, YOND_ERR_EPOLL_CREATE, "epoll init error");
			return err;
		}

		struct epoll_event event, all_events[MAX_EVENTS];
		event.events = EPOLLIN;
		event.data.fd = m_nSockFd;
		if (epoll_ctl(m_nEpollFd, EPOLL_CTL_ADD, m_nSockFd, &event) < 0) {
			err = CYondLog::log(CYondLog::LOG_LEVEL_ERROR, __FILE__, __LINE__, YOND_ERR_EPOLL_CTL, "epoll ctl error");
			return err;
		}

		while (1) {
			int eventMnt = epoll_wait(m_nEpollFd, all_events, MAX_EVENTS, 1000);
			if (eventMnt == -1) {
				err = CYondLog::log(CYondLog::LOG_LEVEL_ERROR, __FILE__, __LINE__, YOND_ERR_EPOLL_WAIT, "epoll wait error");
				break;
			}
			for (int i = 0; i < eventMnt; i++) {
				if (all_events[i].data.fd == m_nSockFd) {
					err = m_handleEvent.addNew(&all_events[i]);
				}
				else {
					err = m_handleEvent.HandleEvent(&all_events[i]);
				}
			}
		}

		close(m_nSockFd);
		close(m_nEpollFd);
		return err;
	};

	int InitSocket() {
		if(m_nSockFd == -1) 
			return CYondLog::log(CYondLog::LOG_LEVEL_ERROR, __FILE__, __LINE__, YOND_ERR_SOCKET_CREATE, "error socket"); // Socket already initialized
		m_nSockFd = socket(AF_INET, SOCK_STREAM, 0);
		memset(&m_addr, 0, sizeof(m_addr));
		m_addr.sin_family = AF_INET;
		m_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		m_addr.sin_port = htons(PORT);
		if (bind(m_nSockFd, (sockaddr*)&m_addr, sizeof(m_addr))) {
			close(m_nSockFd);
			m_nSockFd = -1;
			return CYondLog::log(CYondLog::LOG_LEVEL_ERROR, __FILE__, __LINE__, YOND_ERR_SOCKET_BIND, "bind error");
		}
		if (listen(m_nSockFd, 5)) {
			close(m_nSockFd);
			m_nSockFd = -1;
			return CYondLog::log(CYondLog::LOG_LEVEL_ERROR, __FILE__, __LINE__, YOND_ERR_SOCKET_LISTEN, "listen error");
		}
		if (m_nSockFd < 0) {
			return CYondLog::log(CYondLog::LOG_LEVEL_ERROR, __FILE__, __LINE__, YOND_ERR_SOCKET_CREATE, "socket init error");
		}
		return 0;
	}
	CChatServer() {

	}
private:
	int m_nPort;
	int m_nSockFd;
	sockaddr_in m_addr;
	int m_nEpollFd;
	CYondHandleEvent m_handleEvent;


};

