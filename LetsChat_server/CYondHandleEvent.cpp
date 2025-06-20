#include "CYondHandleEvent.h"

bool CYondHandleEvent::validateSequence(int clientFd, size_t sequence) {
    std::lock_guard<std::mutex> lock(m_sequenceMutex);
    
    auto it = m_clientSequences.find(clientFd);
    if (it == m_clientSequences.end()) {
        // 新客户端，初始化序列号
        m_clientSequences[clientFd] = 0;
        m_messageCache[clientFd] = MessageCache();  // 使用默认构造函数
        return sequence == 0;
    }
    
    // 检查序列号是否在可接受范围内（允许最多跳过10个序列号）
    return sequence >= it->second && sequence <= it->second + 10;
}

void CYondHandleEvent::updateClientSequence(int clientFd, size_t sequence) {
    std::lock_guard<std::mutex> lock(m_sequenceMutex);
    m_clientSequences[clientFd] = sequence + 1;  // 更新为下一个期望的序列号
}

void CYondHandleEvent::handleOutOfOrderPacket(int clientFd, const CYondPack& msg, size_t sequence) {
    MessageCache& cache = m_messageCache[clientFd];
    std::lock_guard<std::mutex> lock(*cache.mutex);
    
    // 缓存乱序消息
    cache.messages[sequence] = msg;
    
    // 检查是否可以处理缓存的消息
    while (!cache.messages.empty()) {
        auto it = cache.messages.begin();
        if (it->first == cache.lastProcessedSeq + 1) {
            // 处理缓存的消息
            ProcessMessage(clientFd, it->second.Data());
            cache.messages.erase(it);
            cache.lastProcessedSeq++;
        } else {
            break;
        }
    }
}

void CYondHandleEvent::requestMissingPackets(int clientFd, size_t expectedSeq, size_t receivedSeq) {
    // 创建重传请求消息
    std::string requestData = std::to_string(expectedSeq) + "," + std::to_string(receivedSeq - 1);
    CYondPack requestMsg(YRetrans, clientFd, requestData.c_str(), requestData.size());
    
    // 发送重传请求
    const std::string& data = requestMsg.Data();
    send(clientFd, data.c_str(), data.size(), 0);
    
    LOG_INFO("Requested retransmission for sequences " + std::to_string(expectedSeq) + 
             " to " + std::to_string(receivedSeq - 1) + " from client " + std::to_string(clientFd));
}

void CYondHandleEvent::ProcessMessage(int clientFd, const CYondPack& msg) {
    if (msg.Size() == 0) {
        LOG_ERROR(YOND_ERR_RECV_PACKET, "Invalid message format");
        return;
    }
    // 处理消息内容
    CYondPack processMsg = msg;

    switch (processMsg.m_sCmd) {
        case YConnect:
            // 处理连接请求
            m_clientFdToIp[clientFd] = processMsg.m_strData;
            LOG_INFO("Client " + m_clientFdToIp[clientFd] + " connected" + " broad login msg!");
            BroadCastToAll(clientFd, processMsg.m_strData, YConnect);
            break;

        case YMsg:
            // 广播消息给所有客户端
            if (!processMsg.m_strData.empty()) {
                LOG_INFO("Broadcasting message from " + m_clientFdToIp[clientFd] + ": " + processMsg.m_strData);
                BroadCastToAll(clientFd, processMsg.m_strData);
            }
            break;

        case YFileStart:
            HandleFileStart(clientFd, processMsg);
            break;

        case YFileData:
            HandleFileData(clientFd, processMsg);
            break;

        case YFileEnd:
            HandleFileEnd(clientFd, processMsg);
            break;

        case YFileAck:
            HandleFileAck(clientFd, processMsg);
            break;

        default:
            LOG_WARNING("Unknown message type: " + std::to_string(processMsg.m_sCmd));
    }
}

void CYondHandleEvent::ExtractAndProcessPackets(int clientFd) {
    std::string& buffer = m_recvBuffer[clientFd];
    while (true) {
        // 至少需要头(2) + 长度(4) + 命令(2) + 用户ID(2)
        if (buffer.size() < 10) break;
        // 查找消息头
        size_t headerPos = std::string::npos;
        for (size_t i = 0; i + 1 < buffer.size(); ++i) {
            uint16_t header;
            memcpy(&header, buffer.data() + i, 2);
            header = ntohs(header);
            if (header == 0xFEFF) {
                headerPos = i;
                break;
            }
        }
        if (headerPos == std::string::npos) {
            // 没有头，丢弃所有数据
            buffer.clear();
            break;
        }
        if (headerPos > 0) {
            buffer.erase(0, headerPos);
        }
        if (buffer.size() < 8) break; // 头(2)+长度(4)+命令(2)
        uint32_t length;
        memcpy(&length, buffer.data() + 2, 4);
        length = ntohl(length);
        size_t totalLen = 2 + 4 + length + 2; // 头+长度+包体+校验和
        if (buffer.size() < totalLen) break; // 不够完整包
        std::string onePacket = buffer.substr(0, totalLen);
        buffer.erase(0, totalLen);
        // 处理完整包
        m_threadPool.Enqueue([this, clientFd, onePacket]() {
            ProcessMessage(clientFd, onePacket);
        });
    }
}