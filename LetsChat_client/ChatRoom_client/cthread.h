#ifndef CTHREAD_H
#define CTHREAD_H
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <condition_variable>
#include <QDebug>
#include <QString>

class CYondThread
{
public:
    CYondThread() : m_hThread(nullptr), m_bIsRunning(false) {
        qDebug() << "Thread instance created";
    }

    ~CYondThread() {
        Stop();
        qDebug() << "Thread instance destroyed";
    }

    template<class F>
    int Start(F&& f) {
        std::unique_lock<std::mutex> lock(m_lock);
        if (m_bIsRunning) {
            qDebug() << "thread had already running!";
            return 2;
        }

        try {
            m_hThread = new std::thread(std::forward<F>(f));
            m_bIsRunning = true;
            qDebug() << "Thread started successfully";
            return 0;
        } catch (const std::exception& e) {
            QString err = "Failed to start thread: " + QString(e.what());
            qDebug() << err;
            return 1;
        }
    }

    int Stop() {
        std::unique_lock<std::mutex> lock(m_lock);
        if (!m_bIsRunning) {
            qDebug() << "Thread is already running";
            return 2;
        }

        if (m_hThread && m_hThread->joinable()) {
            m_hThread->join();
            delete m_hThread;
            m_hThread = nullptr;
        }
        m_bIsRunning = false;
        qDebug() << "Thread stopped";
        return 0;
    }

    bool IsRunning() {
        std::unique_lock<std::mutex> lock(m_lock);
        return m_bIsRunning;
    }

private:
    std::thread* m_hThread;
    std::mutex m_lock;
    bool m_bIsRunning;
};

#endif // CTHREAD_H
