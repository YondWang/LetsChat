#pragma once
#include <string>
#include <string.h>
#include <stdlib.h>
class CYondPack
{
public:
	CYondPack() :m_sHead(0), m_nLength(0), m_sCmd(0), m_sSum(0) {}
	CYondPack(short sCmd, const char* pData, size_t nSize) {
		m_sHead = 0xFEFF;
		m_nLength = nSize + 4;
		m_sCmd = sCmd;
		if (nSize > 0) {
			m_strData.reserve(nSize);
			memcpy((void*)m_strData.c_str(), pData, nSize);
		}
		else {
			m_strData.clear();
		}
		m_sSum = 0;
		for (size_t i = 0; i < m_strData.size(); i++) {
			m_sSum += m_strData[i] & 0xFF;
		}
	}
	CYondPack(const CYondPack& pack) {
		m_sHead = pack.m_sHead;
		m_nLength = pack.m_nLength;
		m_sCmd = pack.m_sCmd;
		m_strData = pack.m_strData;
		m_sSum = pack.m_sSum;
	}
	CYondPack(const char* pData, size_t& nSize) {

	}
private:
	short m_sHead;
	int m_nLength;
	short m_sCmd;
	std::string m_strData;
	short m_sSum;
};