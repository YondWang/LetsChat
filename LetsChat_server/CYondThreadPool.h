#pragma once
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

class CYondThreadPool
{
	CYondThreadPool(size_t size) {

	}

private:
	std::mutex m_lock;
	std::vector<std::thread> m_vThreads;
};

