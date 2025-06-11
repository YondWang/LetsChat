#ifndef CCLIENTPACKET_H
#define CCLIENTPACKET_H

#include <qDebug>
#include <string>


enum YondCmd
{
    YDef,
    YConnect,
    YMsg,
    YFile,
    YRecv
};

class CClientPacket
{
public:
    CClientPacket() :m_sHead(0), m_nLength(0), m_sCmd(YDef), m_sSum(0) {}
    CClientPacket(YondCmd sCmd, const char* pData, size_t nSize) {
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
            m_sSum += (unsigned char)(m_strData[i] & 0xFF);
        }
    }
    CClientPacket(const CClientPacket& pack) {
        m_sHead = pack.m_sHead;
        m_nLength = pack.m_nLength;
        m_sCmd = pack.m_sCmd;
        m_strData = pack.m_strData;
        m_sSum = pack.m_sSum;
    }
    CClientPacket(const unsigned short* pData, size_t& nSize) {
        size_t i = 0;
        for (; i < nSize; i++) {
            if (*(unsigned short*)(pData + i) == 0xFEFF) {
                m_sHead = *(unsigned short*)(pData + i);
                i += 2;
                break;
            }
        }
        if (i + 4 + 2 + 2 + 2 > nSize) {
            nSize = 0;
            qDebug() << "Incomplete packet header!";
            return;
        }
        m_nLength = *(unsigned long*)(pData + i); i += 4;
        if (m_nLength + i > nSize) {
            nSize = 0;
            qDebug() << "Failed to recv the packet!!";
            return;
        }
        m_sCmd = *(YondCmd*)(pData + i); i += 2;
        m_sUser = *(unsigned short*)(pData + i); i += 2;
        if (m_nLength > 4) {
            m_strData.resize(m_nLength - 2 - 2);
            memcpy((void*)m_strData.c_str(), pData + i, m_nLength - 4);
            i += m_nLength - 4;
        }
        m_sSum = *(unsigned short*)(pData + i); i += 2;
        unsigned short sum = 0;
        for (size_t j = 0; j < m_strData.size(); j++) {
            sum += (unsigned char)((m_strData[j]) & 0xFF);
        }
        if (sum == m_sSum) {
            nSize -= i;
            return;
        }
        qDebug() << "Packet sum check error!!";
        nSize = 0;
    }
    ~CClientPacket() {};
    CClientPacket& operator=(const CClientPacket& pack) {
        if (this != &pack) {
            m_sHead = pack.m_sHead;
            m_nLength = pack.m_nLength;
            m_sCmd = pack.m_sCmd;
            m_strData = pack.m_strData;
            m_sSum = pack.m_sSum;
        }
        return *this;
    }
    size_t Size() {
        return m_nLength + 6;
    }
    const char* Data() {
        m_strOut.resize(m_nLength + 6);
        char* pData = (char*)m_strOut.c_str();
        *(unsigned short*)pData = m_sHead; pData += 2;
        *(unsigned long*)pData = m_nLength; pData += 4;
        *(YondCmd*)pData = m_sCmd; pData += 2;
        memcpy(pData, m_strData.c_str(), m_strData.size()); pData += m_strData.size();
        *(unsigned short*)pData = m_sSum;
        return m_strOut.c_str();
    }
public:
    unsigned short m_sHead;
    size_t m_nLength;
    YondCmd m_sCmd;
    short m_sUser;
    std::string m_strData;
    unsigned short m_sSum;
    std::string m_strOut;
};

#endif // CCLIENTPACKET_H
