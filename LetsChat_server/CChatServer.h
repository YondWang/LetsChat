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
#include <atomic>
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

	int InitSocket();

	int StartService() {
		int err = 0;
		err = InitSocket();

		m_nEpollFd = epoll_create1(0);
		if (m_nEpollFd < 0) {
			return LOG_ERROR(YOND_ERR_EPOLL_CREATE, "Failed to create epoll instance");
		}

		m_handleEvent = std::make_unique<CYondHandleEvent>(m_nEpollFd);

		struct epoll_event event;
		event.events = EPOLLIN;
		event.data.fd = m_nSockFd;
		if (epoll_ctl(m_nEpollFd, EPOLL_CTL_ADD, m_nSockFd, &event) < 0) {
			return LOG_ERROR(YOND_ERR_EPOLL_CTL, "Failed to add socket to epoll");
		}

		LOG_INFO("Server started, waiting for connections...");

		// 创建并启动Epoll线程
		m_threadPool = std::make_unique<CYondThreadPool>(m_nEpollFd);
		if (!m_threadPool->Invoke()) {
			return LOG_ERROR(YOND_ERR_THREAD_CREATE, "Failed to start epoll thread");
		}

		// 主线程等待用户输入
		printf("Press any key to stop the server...\n");
		getchar();

		// 停止服务器
		StopServer();

		return err;
	}

	void StopServer() {
		LOG_INFO("Stopping server...");
		
		// 设置停止标志
		m_stopFlag = true;

		// 停止线程池
		m_threadPool->Stop();

		// 关闭socket和epoll
		if (m_nSockFd != -1) {
			close(m_nSockFd);
			m_nSockFd = -1;
		}
		if (m_nEpollFd != -1) {
			close(m_nEpollFd);
			m_nEpollFd = -1;
		}

		LOG_INFO("Server stopped");
	}

	CChatServer() : m_nSockFd(-1), m_nEpollFd(-1), m_stopFlag(false) {
		LOG_INFO("Chat server instance created");
	}

	~CChatServer() {
		StopServer();
	}

private:
	void threadEpoll() {
		struct epoll_event all_events[MAX_EVENTS];
		while (!m_stopFlag) {
			int eventMnt = epoll_wait(m_nEpollFd, all_events, MAX_EVENTS, 1000);
			if (eventMnt == -1) {
				if (errno == EINTR) continue;  // 被信号中断，继续等待
				LOG_ERROR(YOND_ERR_EPOLL_WAIT, "Failed to wait for epoll events");
				continue;
			}

			for (int i = 0; i < eventMnt; i++) {
				if (all_events[i].data.fd == m_nSockFd) {
					m_handleEvent->addNew(&all_events[i]);
				}
				else {
					m_handleEvent->HandleEvent(&all_events[i]);
				}
			}
		}
	}

private:
	static CChatServer* m_instance;
	int m_nPort;
	int m_nSockFd;
	sockaddr_in m_addr;
	int m_nEpollFd;
	std::unique_ptr<CYondThreadPool> m_threadPool;
	std::unique_ptr<CYondHandleEvent> m_handleEvent;
	std::atomic<bool> m_stopFlag;
};

