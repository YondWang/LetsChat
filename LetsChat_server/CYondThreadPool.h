#pragma once
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <condition_variable>
#include <atomic>
#include "CYondLog.h"
#include "CYondHandleEvent.h"

//class CYondHandleEvent;

// 基础线程类，用于封装具体的业务线程逻辑
class CYondThread {
public:
	CYondThread() : m_bIsRunning(false) {
		LOG_INFO("Thread instance created");
	}
	
	~CYondThread() {
		Stop();
		LOG_INFO("Thread instance destroyed");
	}

	void SetWorker(std::function<void()> worker) {
		m_worker = std::move(worker);
	}

	bool Start() {
		std::unique_lock<std::mutex> lock(m_lock);
		if (m_bIsRunning) {
			LOG_ERROR(YOND_ERR_THREAD_CREATE, "Thread is already running");
			return false;
		}

		m_bIsRunning = true;
		m_thread = std::thread([this]() {
			LOG_INFO("Thread started");
			if (m_worker) {
				m_worker();
			}
			this->Stop();
		});
		
		return true;
	}

	void Stop() {
		std::unique_lock<std::mutex> lock(m_lock);
		if (!m_bIsRunning) return;

		if (m_thread.joinable()) {
			m_thread.join();
		}
		m_bIsRunning = false;
		LOG_INFO("Thread stopped");
	}

	bool IsRunning() {
		std::unique_lock<std::mutex> lock(m_lock);
		return m_bIsRunning;
	}

private:
	std::thread m_thread;
	std::mutex m_lock;
	bool m_bIsRunning;
	int m_epollFd;
	std::function<void()> m_worker;
	//CYondHandleEvent* m_pHandleEvent;
};

// 线程池类，用于管理线程资源
class CYondThreadPool {
public:
	CYondThreadPool(size_t threads = 6) : m_bStop(false) {
		for (size_t i = 0; i < threads; ++i) {
			m_vpThreads.push_back(new CYondThread());
		}
		LOG_INFO("Thread pool initialized with " + std::to_string(threads) + " worker threads");
	}

	~CYondThreadPool() {
		Stop();
		for (auto thread : m_vpThreads) {
			delete thread;
		}
		m_vpThreads.clear();
	}

	int Invoke() {
		int err = 0;
		for (size_t i = 0; i < m_vpThreads.size(); i++) {
			if (m_vpThreads[i]->Start() == false) {
				err = LOG_ERROR(YOND_ERR_THREADPOOL_INVOKE, "Error invoke thread in threadpool");
				break;
			}
		}
		if (err != 0) {
			for (size_t i = 0; i < m_vpThreads.size(); i++) {
				m_vpThreads[i]->Stop();
			}
		}
		return err;
	}

	int Stop() {
		for (size_t i = 0; i < m_vpThreads.size(); i++) {
			m_vpThreads[i]->Stop();
		}
		return LOG_INFO("Thread pool destroyed");
	}

	template<class F>
	void Enqueue(F&& f) {
		{
			std::unique_lock<std::mutex> lock(m_lock);
			m_tasks.push(std::forward<F>(f));
		}
		m_condition.notify_one();
	}

	void SetThreadWorker(size_t index, std::function<void()> worker) {
		if (index < m_vpThreads.size()) {
			m_vpThreads[index]->SetWorker(std::move(worker));
		}
	}

	bool IsStopped() const {
		return m_bStop;
	}

private:
	void WorkerThread() {
		LOG_INFO("Worker thread started");
		while (!m_bStop) {
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(m_lock);
				m_condition.wait(lock, [this] { 
					return m_bStop || !m_tasks.empty(); 
				});
				
				if (m_bStop && m_tasks.empty()) {
					LOG_INFO("Worker thread stopping");
					return;
				}
				
				task = std::move(m_tasks.front());
				m_tasks.pop();
			}
			task();
		}
	}

	std::mutex m_lock;
	std::condition_variable m_condition;
	std::vector<CYondThread*> m_vpThreads;
	std::queue<std::function<void()>> m_tasks;
	std::atomic<bool> m_bStop;
};