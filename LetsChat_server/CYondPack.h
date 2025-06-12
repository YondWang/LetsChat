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

	YNULL
};

class CYondPack
{
public:
	CYondPack() :m_sHead(0), m_nLength(0), m_sCmd(YNULL), m_sUser(0), m_sSum(0) {}
	CYondPack(YondCmd sCmd, unsigned short userFd, const char* pData, size_t nSize) {
		m_sHead = 0xFEFF;
		m_nLength = nSize + 4;
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
		for (size_t i = 0; i < m_strData.size(); ++i) {
			m_sSum += (unsigned char)(m_strData[i]);
		}
	}
	CYondPack(const CYondPack& pack) {
		m_sHead = pack.m_sHead;
		m_nLength = pack.m_nLength;
		m_sCmd = pack.m_sCmd;
		m_sUser = pack.m_sUser;
		m_strData = pack.m_strData;
		m_sSum = pack.m_sSum;
	}
	CYondPack(const unsigned char* pData, size_t& nSize) {
		// 打印接收到的原始数据
		LOG_INFO("Received raw data:");
		for (size_t i = 0; i < nSize; i++) {
			printf("%02X ", pData[i]);
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
		uint32_t len32;
		memcpy(&len32, pData + i, sizeof(len32));
		m_nLength = ntohl(len32);
		i += 4;
		if (m_nLength + i > nSize) {
			nSize = 0;
			LOG_ERROR(YOND_ERR_RECV_PACKET, "Failed to recv the packet!!");
			return;
		}
		uint16_t cmd16;
		memcpy(&cmd16, pData + i, sizeof(cmd16));
		m_sCmd = (YondCmd)ntohs(cmd16);
		i += 2;

		uint16_t user16;
		memcpy(&user16, pData + i, sizeof(user16));
		m_sUser = ntohs(user16);
		i += 2;

		if (m_nLength > 4) {
			m_strData.resize(m_nLength - 2 - 2);
			memcpy(&m_strData[0], pData + i, m_nLength - 4);
			i += m_nLength - 4;
		}
		uint16_t sum16;
		memcpy(&sum16, pData + i, sizeof(sum16));
		m_sSum = ntohs(sum16);
		unsigned short sum = 0;
		for (size_t j = 0; j < m_strData.size(); j++) {
			sum += (unsigned char)((m_strData[j]) & 0xFF);
		}
		if (sum == m_sSum) {
			nSize -= i;
			LOG_INFO("Sum check is success:" + std::to_string(m_sSum));
			return;
		}
		LOG_ERROR(YOND_ERR_PACKET_SUMCHECK, "Packet sum check error!!");
		nSize = 0;
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
	size_t Size() {
		return m_nLength + 8;
	}
	std::string Data() {
		m_strOut.resize(m_nLength + 8);  // 总长度：头2 + 长度4 + 命令2 + 用户2 + 数据 + 校验和2
		char* pData = &m_strOut[0];      // 获取可写指针

		// 拼接数据
		memcpy(pData, &m_sHead, 2); pData += 2;
		memcpy(pData, &m_nLength, 4); pData += 4;
		memcpy(pData, &m_sCmd, 2); pData += 2;
		memcpy(pData, &m_sUser, 2); pData += 2;

		if (!m_strData.empty()) {
			memcpy(pData, m_strData.data(), m_strData.size());
			pData += m_strData.size();
		}

		memcpy(pData, &m_sSum, 2);  // 最后写入校验和

		LOG_INFO("Broad raw data:");
		for (size_t i = 0; i < this->Size(); i++) {
			printf("%02X ", (unsigned char)m_strOut[i]);
		}
		printf("\r\n");

		return m_strOut;
	}
public:
	unsigned short m_sHead;
	size_t m_nLength;
	YondCmd m_sCmd;
	unsigned short m_sUser;
	std::string m_strData;
	unsigned short m_sSum;
	std::string m_strOut;
};