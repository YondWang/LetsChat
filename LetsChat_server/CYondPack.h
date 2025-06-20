#pragma once
#include <string>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>  // ntohs, ntohl
#include <cstring>      // memcpy
#include <cstdint>
#include "CYondLog.h"

enum YondCmd
{
	YConnect,
	YMsg,
	YFile,
	YRecv,
	YDisCon,
	YFileStart,    // 文件传输开始
	YFileData,     // 文件数据包
	YFileEnd,      // 文件传输结束
	YFileAck,      // 文件传输确认
	YRetrans,      // 请求重传
	YUserList = 10, // 新增：用户列表
	YNULL
};

class CYondPack
{
public:
	CYondPack() :m_sHead(0), m_nLength(0), m_sCmd(YNULL), m_sUser(0), m_sSum(0) {}
	CYondPack(YondCmd sCmd, unsigned short userFd, const char* pData, size_t nSize) {
		m_sHead = 0xFEFF;
		m_nLength = nSize + 4;  // 数据长度 + 命令(2) + 用户ID(2)
		m_sCmd = sCmd;
		m_sUser = userFd;
		if (nSize > 0) {
			m_strData.resize(nSize);
			memcpy(&m_strData[0], pData, nSize);
		}
		else {
			m_strData.clear();
		}
		m_sSum = 0;
		// 只对数据内容做校验和，跳过前2字节序列号
		for (size_t i = 2; i < m_strData.size(); ++i) {
			m_sSum += (unsigned char)(m_strData[i]);
		}
	}
	CYondPack(const char* pData, size_t& nSize) {
		// 打印接收到的原始数据
		LOG_INFO("Received raw data:");
		for (size_t i = 0; i < nSize; i++) {
			printf("%02X ", (unsigned char)pData[i]);
		}
		printf("\r\n");

		// 查找消息头部
		size_t i = 0;
		for (; i < nSize - 1; i++) {
			uint16_t header;
			memcpy(&header, pData + i, sizeof(header));
			header = ntohs(header);

			if (header == 0xFEFF) {
				m_sHead = header;
				LOG_INFO("Found message header 0xFEFF at position" + std::to_string(i));
				i += 2;
				break;
			}
		}
		if (i + 4 + 2 + 2 + 2 > nSize) {
			nSize = 0;
			LOG_ERROR(YOND_ERR_RECV_PACKET, "Incomplete packet header!");
			return;
		}

		// 读取长度
		uint32_t len32;
		memcpy(&len32, pData + i, sizeof(len32));
		m_nLength = ntohl(len32);
		i += 4;

		if (m_nLength + i > nSize) {
			nSize = 0;
			LOG_ERROR(YOND_ERR_RECV_PACKET, "Failed to read packet body!");
			return;
		}

		// 读取命令
		uint16_t cmd16;
		memcpy(&cmd16, pData + i, sizeof(cmd16));
		m_sCmd = (YondCmd)ntohs(cmd16);
		i += 2;

		// 读取用户ID
		uint16_t user16;
		memcpy(&user16, pData + i, sizeof(user16));
		m_sUser = ntohs(user16);
		i += 2;

		// 读取数据
		size_t dataLen = m_nLength - 4;  // 减去命令和用户ID的长度
		if (dataLen > 0) {
			m_strData.resize(dataLen);
			memcpy(&m_strData[0], pData + i, dataLen);
			i += dataLen;
		}
		else {
			m_strData.clear();
		}

		// 读取校验和
		if (i + 2 <= nSize) {
			uint16_t sum16;
			memcpy(&sum16, pData + i, sizeof(sum16));
			m_sSum = ntohs(sum16);
			i += 2;
		}
		else {
			m_sSum = 0;
		}

		uint16_t tsum = 0;
		for (size_t i = 0; i < m_strData.size(); i++) {
			tsum += (unsigned char)(m_strData[i] & 0xFF);
		}
		if (tsum == m_sSum) {
			nSize = i;
			return;
		}
		LOG_ERROR(YOND_ERR_PACKET_SUMCHECK, "Sumcheck error!!!");
	}
	~CYondPack() {};
	CYondPack& operator=(const CYondPack& pack) {
		if (this != &pack) {
			m_sHead = pack.m_sHead;
			m_nLength = pack.m_nLength;
			m_sCmd = pack.m_sCmd;
			m_sUser = pack.m_sUser;
			m_strData = pack.m_strData;
			m_sSum = pack.m_sSum;
		}
		return *this;
	}
	size_t Size() const{
		// 总长度：头部(2) + 长度(4) + 命令(2) + 用户ID(2) + 数据 + 校验和(2)
		return 2 + 4 + 2 + 2 + m_strData.size() + 2;
	}
	const std::string& Data() const {
		static std::string data;
		data.clear();
		
		// 消息头 (2字节)
		uint16_t head = htons(m_sHead);
		data.append((char*)&head, sizeof(head));
		
		// 消息长度 (4字节)
		uint32_t len = htonl(m_nLength);
		data.append((char*)&len, sizeof(len));
		
		// 命令 (2字节)
		uint16_t cmd = htons((uint16_t)m_sCmd);
		data.append((char*)&cmd, sizeof(cmd));
		
		// 用户ID (2字节)
		uint16_t user = htons(m_sUser);
		data.append((char*)&user, sizeof(user));
		
		// 数据
		if (!m_strData.empty()) {
			data.append(m_strData);
		}
		
		// 校验和 (2字节)
		uint16_t sum = htons(m_sSum);
		data.append((char*)&sum, sizeof(sum));
		
		return data;
	}

	// 添加一个辅助方法来打印消息内容
	void PrintMessage() {
		std::string hexData;
		const char* data = Data().data();
		size_t size = Size();
		for (size_t i = 0; i < size; i++) {
			char hex[4];
			snprintf(hex, sizeof(hex), "%02X ", (unsigned char)data[i]);
			hexData += hex;
		}
		LOG_INFO("Message content: " + hexData);
	}

public:
	unsigned short m_sHead;
	size_t m_nLength;  // 包含命令和用户ID的长度
	YondCmd m_sCmd;
	unsigned short m_sUser;
	std::string m_strData;
	unsigned short m_sSum;
};