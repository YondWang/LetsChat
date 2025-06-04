#pragma once
#include <sys/socket.h>
#include "CYondErr.h"
#include <netinet/in.h>
#include <sys/epoll.h>
class CChatServer
{
public:
	int StartService() {
		m_nSockFd = socket(AF_INET, SOCK_STREAM, 0);
		m_nEpollFd = epoll_create(1);
		if (m_nEpollFd < 0) {
			return YOND_ERR_SOCKET_EPOLL_CREATE; // Error creating epoll instance
		}
		if (m_nSockFd < 0) {
			return YOND_ERR_SOCKET_CREATE; // Error creating socket
		}
		if (bind(m_nSockFd, (sockaddr*)&m_addr, sizeof(m_addr))) {
			shutdown(m_nSockFd, SHUT_RDWR);
			m_nSockFd = -1;
			return YOND_ERR_SOCKET_CREATE; // Error binding socket
		}
		if (listen(m_nSockFd, 5)) {
			shutdown(m_nSockFd, SHUT_RDWR);
			m_nSockFd = -1;
			return YOND_ERR_SOCKET_LISTEN; // Error listening on socket
		}
	}
	CChatServer() {

	}
private:
	int m_nPort;
	int m_nSockFd;
	sockaddr_in m_addr;
	int m_nEpollFd;
};

