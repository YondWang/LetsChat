//#ifndef CCLIENTPACKET_H
//#define CCLIENTPACKET_H

//#pragma once
//#include <string>
//#include <string.h>
//#include <stdlib.h>
//#include <cstring>      // memcpy
//#include <cstdint>
//#include <QDebug>

//enum YondCmd
//{
//	YConnect,
//	YMsg,
//	YFile,
//	YRecv,

//	YNULL
//};

//class CYondPack
//{
//public:
//	CYondPack() :m_sHead(0), m_nLength(0), m_sCmd(YNULL), m_sUser(NULL), m_sSum(0) {}
//	CYondPack(YondCmd sCmd, const char* pData, size_t nSize) {
//		m_sHead = 0xFEFF;
//		m_nLength = nSize + 4;
//		m_sCmd = sCmd;
//		if (nSize > 0) {
//			m_strData.reserve(nSize);
//			memcpy((void*)m_strData.c_str(), pData, nSize);
//		}
//		else {
//			m_strData.clear();
//		}
//		m_sSum = 0;
//		for (size_t i = 0; i < m_strData.size(); i++) {
//			m_sSum += (unsigned char)(m_strData[i] & 0xFF);
//		}
//	}
//	CYondPack(const CYondPack& pack) {
//		m_sHead = pack.m_sHead;
//		m_nLength = pack.m_nLength;
//		m_sCmd = pack.m_sCmd;
//		m_strData = pack.m_strData;
//		m_sSum = pack.m_sSum;
//	}
//	CYondPack(const unsigned char* pData, size_t& nSize) {
//		// 打印接收到的原始数据
//		qDebug() << "Received raw data:";
//		for (size_t i = 0; i < nSize; i++) {
//			printf("%02X ", pData[i]);
//		}
//		printf("\r\n");

//		// 查找消息头部
//		size_t i = 0;
//		for (; i < nSize - 1; i++) {
//			uint16_t header;
//			memcpy(&header, pData + i, sizeof(header));
//			header = ntohs(header);

//			if (header == 0xFEFF) {
//				m_sHead = header;
//				qDebug() << "Found message header 0xFEFF at position" << i;
//				i += 2;
//				break;
//			}
//		}
//		if (i + 4 + 2 + 2 + 2 > nSize) {
//			nSize = 0;
//			qDebug() << "Incomplete packet header!";
//			return;
//		}
//		uint32_t len32;
//		memcpy(&len32, pData + i, sizeof(len32));
//		m_nLength = ntohl(len32);
//		i += 4;
//		if (m_nLength + i > nSize) {
//			nSize = 0;
//			qDebug() << "Failed to recv the packet!!";
//			return;
//		}
//		uint16_t cmd16;
//		memcpy(&cmd16, pData + i, sizeof(cmd16));
//		m_sCmd = (YondCmd)ntohs(cmd16);
//		i += 2;

//		uint16_t user16;
//		memcpy(&user16, pData + i, sizeof(user16));
//		m_sUser = ntohs(user16);
//		i += 2;

//		if (m_nLength > 4) {
//			m_strData.resize(m_nLength - 2 - 2);
//			memcpy((void*)m_strData.c_str(), pData + i, m_nLength - 4);
//			i += m_nLength - 4;
//		}
//		uint16_t sum16;
//		memcpy(&sum16, pData + i, sizeof(sum16));
//		m_sSum = htons(sum16);
//		unsigned short sum = 0;
//		for (size_t j = 0; j < m_strData.size(); j++) {
//			sum += (unsigned char)((m_strData[j]) & 0xFF);
//		}
//		if (sum == m_sSum) {
//			nSize -= i;
//			qDebug() << "Sum check is success:" << m_sSum;
//			return;
//		}
//		qDebug() << "Packet sum check error!!";
//		nSize = 0;
//	}
//	~CYondPack() {};
//	CYondPack& operator=(const CYondPack& pack) {
//		if (this != &pack) {
//			m_sHead = pack.m_sHead;
//			m_nLength = pack.m_nLength;
//			m_sCmd = pack.m_sCmd;
//			m_strData = pack.m_strData;
//			m_sSum = pack.m_sSum;
//		}
//		return *this;
//	}
//	size_t Size() {
//		return m_nLength + 6;
//	}
//	std::string Data() {
//		m_strOut.resize(m_nLength + 6);
//		char* pData = (char*)m_strOut.c_str();
//		*(unsigned short*)pData = m_sHead; pData += 2;
//		*(unsigned long*)pData = m_nLength; pData += 4;
//		*(YondCmd*)pData = m_sCmd; pData += 2;
//		memcpy(pData, m_strData.c_str(), m_strData.size()); pData += m_strData.size();
//		*(unsigned short*)pData = m_sSum;
//		return m_strOut;
//	}
//public:
//	unsigned short m_sHead;
//	size_t m_nLength;
//	YondCmd m_sCmd;
//	short m_sUser;
//	std::string m_strData;
//	unsigned short m_sSum;
//	std::string m_strOut;
//};

//#endif // CCLIENTPACKET_H
