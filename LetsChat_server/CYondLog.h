#pragma once
#include <stdio.h>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <linux/limits.h>
//return CYondLog::log(CYondLog::LOG_LEVEL_ERROR, __FILE__, __LINE__, YOND_ERR_SOCKET_CREATE, "socket init error");
#pragma once
#ifndef _COMMON_ERROR_CODE_H_
#define _COMMON_ERROR_CODE_H_

typedef int YondErrCode;

const YondErrCode YOND_ERR_OK = 0; // No error
const YondErrCode YOND_ERR_SOCKET_CREATE = 2000; // Error creating socket
const YondErrCode YOND_ERR_SOCKET_BIND = 2001; // Error binding socket
const YondErrCode YOND_ERR_SOCKET_LISTEN = 2002; // Error listening on socket
const YondErrCode YOND_ERR_SOCKET_ACCEPT = 2003; // Error accepting connection
const YondErrCode YOND_ERR_SOCKET_SEND = 2004; // Error sending data
const YondErrCode YOND_ERR_SOCKET_RECV = 2005; // Error receiving data

const YondErrCode YOND_ERR_EPOLL_CREATE = 2006; // Error creating epoll instance
const YondErrCode YOND_ERR_EPOLL_CTL = 2007; // Error adding or modifying epoll event
const YondErrCode YOND_ERR_EPOLL_WAIT = 2008; // Error waiting for epoll events

const YondErrCode YOND_ERR_THREAD_CREATE = 2009; // Error creating thread

const YondErrCode YOND_ERR_RECV_PACKET = 2050;	//Error recv packet
const YondErrCode YOND_ERR_PACKET_SUMCHECK = 2051;	//Error packet sumCheck
const YondErrCode ERR_LOG_THREAD_TASK = 2052;	//Error thread task

#endif // !_COMMON_ERROR_CODE_H_

class CYondLog
{
public:
	enum LogLevel {
		LOG_LEVEL_INFO,
		LOG_LEVEL_WARNING,
		LOG_LEVEL_ERROR
	};

	static bool Initialize(const std::string& logDir = "logs") {
		static std::mutex initMutex;
		std::lock_guard<std::mutex> lock(initMutex);

		if (m_initialized) {
			return true;
		}

		try {
			// 获取可执行文件的路径
			char exePath[PATH_MAX];
			ssize_t count = readlink("/proc/self/exe", exePath, PATH_MAX);
			if (count == -1) {
				printf("Failed to get executable path\n");
				return false;
			}
			exePath[count] = '\0';

			// 获取可执行文件所在目录
			std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
			
			// 构建日志目录的绝对路径
			std::filesystem::path logDirPath = exeDir / logDir;
			
			// 创建日志目录
			std::filesystem::create_directories(logDirPath);
			
			// 生成日志文件名（使用当前日期）
			auto now = std::chrono::system_clock::now();
			auto time = std::chrono::system_clock::to_time_t(now);
			std::stringstream ss;
			ss << logDirPath.string() << "/chat_server_" 
			   << std::put_time(std::localtime(&time), "%Y%m%d") << ".log";
			
			m_logFile.open(ss.str(), std::ios::app);
			if (!m_logFile.is_open()) {
				printf("Failed to open log file: %s\n", ss.str().c_str());
				return false;
			}

			// 记录日志文件路径
			printf("Log file created at: %s\n", ss.str().c_str());
			m_initialized = true;
			return true;
		} catch (const std::exception& e) {
			printf("Failed to initialize logger: %s\n", e.what());
			return false;
		}
	}

	static void Shutdown() {
		if (m_logFile.is_open()) {
			m_logFile.close();
		}
		m_initialized = false;
	}

	// 获取当前时间字符串
	static std::string GetCurrentTime() {
		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch()) % 1000;

		std::stringstream ss;
		ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
		   << '.' << std::setfill('0') << std::setw(3) << ms.count();
		return ss.str();
	}

	// 获取日志级别字符串
	static const char* GetLogLevelString(LogLevel level) {
		switch (level) {
			case LOG_LEVEL_INFO: return "INFO";
			case LOG_LEVEL_WARNING: return "WARNING";
			case LOG_LEVEL_ERROR: return "ERROR";
			default: return "UNKNOWN";
		}
	}

	// 获取错误码描述
	static std::string GetErrorDescription(YondErrCode err) {
		switch (err) {
			case YOND_ERR_OK: return "No error";
			case YOND_ERR_SOCKET_CREATE: return "Error creating socket";
			case YOND_ERR_SOCKET_BIND: return "Error binding socket";
			case YOND_ERR_SOCKET_LISTEN: return "Error listening on socket";
			case YOND_ERR_SOCKET_ACCEPT: return "Error accepting connection";
			case YOND_ERR_SOCKET_SEND: return "Error sending data";
			case YOND_ERR_SOCKET_RECV: return "Error receiving data";
			case YOND_ERR_EPOLL_CREATE: return "Error creating epoll instance";
			case YOND_ERR_EPOLL_CTL: return "Error adding or modifying epoll event";
			case YOND_ERR_EPOLL_WAIT: return "Error waiting for epoll events";
			case YOND_ERR_THREAD_CREATE: return "Error creating thread";
			case YOND_ERR_RECV_PACKET: return "Error recv packet";
			case YOND_ERR_PACKET_SUMCHECK: return "Error packet sum check";
			default: return "Unknown error code";
		}
	}

	// 格式化日志消息
	static std::string FormatLogMessage(LogLevel level, const char* file, int line, 
									  YondErrCode err, const std::string& msg) {
		std::stringstream ss;
		ss << "[" << GetCurrentTime() << "] "
		   << "[" << GetLogLevelString(level) << "] "
		   << "[" << file << ":" << line << "] ";
		
		if (err != YOND_ERR_OK) {
			ss << "[ERR:" << err << " - " << GetErrorDescription(err) << "] ";
		}
		
		ss << msg;
		return ss.str();
	}

	// 主日志函数
	static int Log(LogLevel level, const char* file, int line, YondErrCode err, 
				  const std::string& msg) {
		static std::mutex logMutex;
		std::lock_guard<std::mutex> lock(logMutex);

		if (!m_initialized && !Initialize()) {
			printf("Logger not initialized\n");
			return err;
		}

		std::string logMsg = FormatLogMessage(level, file, line, err, msg);
		
		// 写入文件
		m_logFile << logMsg << std::endl;
		m_logFile.flush();

		// 同时输出到控制台（带颜色）
		switch (level) {
			case LOG_LEVEL_INFO:
				printf("\033[32m%s\033[0m\n", logMsg.c_str()); // 绿色
				break;
			case LOG_LEVEL_WARNING:
				printf("\033[33m%s\033[0m\n", logMsg.c_str()); // 黄色
				break;
			case LOG_LEVEL_ERROR:
				printf("\033[31m%s\033[0m\n", logMsg.c_str()); // 红色
				break;
			default:
				printf("%s\n", logMsg.c_str());
		}

		return err;
	}

	// 便捷日志函数
	static int Info(const char* file, int line, const std::string& msg) {
		return Log(LOG_LEVEL_INFO, file, line, YOND_ERR_OK, msg);
	}

	static int Warning(const char* file, int line, const std::string& msg) {
		return Log(LOG_LEVEL_WARNING, file, line, YOND_ERR_OK, msg);
	}

	static int Error(const char* file, int line, YondErrCode err, const std::string& msg) {
		return Log(LOG_LEVEL_ERROR, file, line, err, msg);
	}

private:
	inline static std::ofstream m_logFile;
	inline static bool m_initialized;
};
// 便捷宏定义
#define LOG_INFO(msg) CYondLog::Info(__FILE__, __LINE__, msg)
#define LOG_WARNING(msg) CYondLog::Warning(__FILE__, __LINE__, msg)
#define LOG_ERROR(err, msg) CYondLog::Error(__FILE__, __LINE__, err, msg)