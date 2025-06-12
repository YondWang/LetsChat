#pragma once
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <condition_variable>
#include "CYondLog.h"

class CYondThread {
public:
	CYondThread() : m_hThread(nullptr), m_bIsRunning(false) {
		LOG_INFO("Thread instance created");
	}

	~CYondThread() {
		Stop();
		LOG_INFO("Thread instance destroyed");
	}

	template<class F>
	int Start(F&& f) {
		std::unique_lock<std::mutex> lock(m_lock);
		if (m_bIsRunning) {
			return LOG_WARNING("Thread is already running");
		}

		try {
			m_hThread = new std::thread(std::forward<F>(f));
			m_bIsRunning = true;
			LOG_INFO("Thread started successfully");
			return 0;
		}
		catch (const std::exception& e) {
			return LOG_ERROR(YOND_ERR_THREAD_CREATE, "Failed to start thread: " + std::string(e.what()));
		}
	}

	int Stop() {
		std::unique_lock<std::mutex> lock(m_lock);
		if (!m_bIsRunning) {
			return LOG_WARNING("Thread is already running");
		}

		if (m_hThread) {
			if(m_hThread->joinable()) {
				m_hThread->join();
			}
		delete m_hThread;
		m_hThread = nullptr;
		}
		m_bIsRunning = false;
		return LOG_INFO("Thread stopped");
	}

	bool IsRunning() {
		std::unique_lock<std::mutex> lock(m_lock);
		return m_bIsRunning;
	}

	int Detach() {
		m_hThread->detach();
		return 0;
	}

private:
	std::thread* m_hThread;
	std::mutex m_lock;
	bool m_bIsRunning;
};

class CYondThreadPool
{
public:
	CYondThreadPool(size_t threads = 6) : m_bStop(false) {
		m_vThreads.resize(threads);
		for (size_t i = 0; i < m_vThreads.size(); ++i) {
			m_vThreads[i] = new CYondThread();
			m_vThreads[i]->Start([this]() {
				this->WorkerThread(); 
			});
		}
		LOG_INFO("Thread pool initialized with " + std::to_string(threads) + " worker threads");
	}

	~CYondThreadPool() {
		{
			std::unique_lock<std::mutex> lock(m_lock);
			m_bStop = true;
		}
		m_condition.notify_all();
		
		for (size_t i = 0; i < m_vThreads.size(); ++i) {
			if (m_vThreads[i]->IsRunning()) {
				m_vThreads[i]->Stop();
			}
			delete m_vThreads[i];
		}
		/*for (auto& thread : m_vThreads) {
			if (thread->joinable()) {
				thread->join();
			}
			delete thread;
		}*/
		m_vThreads.clear();
		LOG_INFO("Thread pool destroyed");
	}

	template<class F>
	void Enqueue(F&& f) {
		{
			std::unique_lock<std::mutex> lock(m_lock);
			m_tasks.emplace(std::forward<F>(f));
		}
		m_condition.notify_one();
	}

private:
	void WorkerThread() {
		LOG_INFO("Worker thread started");
		while (true) {
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(m_lock);
				m_condition.wait(lock, [this] { 
					return m_bStop || !m_tasks.empty(); 
				});
				
				if (m_bStop && m_tasks.empty()) {
					LOG_INFO("Worker thread stopping");
					break;
				}
				
				task = std::move(m_tasks.front());
				m_tasks.pop();
			}
			try {
				task();
			}
			catch(const std::exception& e){
				LOG_ERROR(ERR_LOG_THREAD_TASK, "exception thread task:" + std::string(e.what()));
			}
		}
	}

	std::mutex m_lock;
	std::condition_variable m_condition;
	std::vector<CYondThread*> m_vThreads;
	std::queue<std::function<void()>> m_tasks;
	bool m_bStop;
};

