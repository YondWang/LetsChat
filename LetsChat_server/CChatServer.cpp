#include "CChatServer.h"

CChatServer* CChatServer::m_instance = NULL;

int CChatServer::EpollDo(epoll_event* alevt) {
	int err = 0;
	while (!m_bStop) {
		int eventMnt = epoll_wait(m_nEpollFd, alevt, MAX_EVENTS, 1000);
		if (eventMnt == -1) {
			return LOG_ERROR(YOND_ERR_EPOLL_WAIT, "Failed to wait for epoll events");
		}
		for (int i = 0; i < eventMnt; i++) {
			if (alevt[i].data.fd == m_nSockFd) {
				err = m_handleEvent.addNew(&alevt[i]);
			}
			else {
				err = m_handleEvent.HandleEvent(&alevt[i]);
			}
		}
	}
	return err;
}

int CChatServer::StartService() {
	int err = 0;
	err = InitSocket();

	m_nEpollFd = epoll_create1(0);
	if (m_nEpollFd < 0) {
		return LOG_ERROR(YOND_ERR_EPOLL_CREATE, "Failed to create epoll instance");
	}

	struct epoll_event event, all_events[MAX_EVENTS];
	event.events = EPOLLIN;
	event.data.fd = m_nSockFd;
	if (epoll_ctl(m_nEpollFd, EPOLL_CTL_ADD, m_nSockFd, &event) < 0) {
		return LOG_ERROR(YOND_ERR_EPOLL_CTL, "Failed to add socket to epoll");
	}

	LOG_INFO("Server started, waiting for connections...");

	std::thread epollThread([this, &all_events]() {
		return EpollDo(all_events);
	});
	epollThread.detach();
	
	getchar();
	m_bStop = true;
	close(m_nSockFd);
	close(m_nEpollFd);
	return err;
}
