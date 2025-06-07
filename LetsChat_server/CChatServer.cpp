#include "CChatServer.h"

CChatServer* CChatServer::m_instance = NULL;

int CChatServer::InitSocket() {
	if (m_nSockFd != -1)
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
