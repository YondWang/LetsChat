#include "CYondHandleEvent.h"

void CYondHandleEvent::ProcessMessage(int clientFd, const CYondPack& msg) {
    if (msg.Size() == 0) {
        LOG_ERROR(YOND_ERR_RECV_PACKET, "Invalid message format");
        return;
    }

    // 处理消息内容
    CYondPack processMsg = msg;

    switch (msg.m_sCmd) {
    case YConnect: {
        // 记录新客户端
        m_clientFdToIp[clientFd] = msg.m_strData;
        LOG_INFO("Client " + m_clientFdToIp[clientFd] + " connected" + " broad login msg!");
        // 发送所有在线用户列表
        std::string userList;
        for (const auto& kv : m_clientFdToIp) {
            unsigned short uid = (unsigned short)(kv.first & 0xFFFF);
            userList += std::to_string(uid) + "|" + kv.second + ";";
        }
        if (!userList.empty()) userList.pop_back(); // 去掉最后一个分号
        CYondPack userListMsg(YUserList, clientFd, userList.c_str(), userList.size());
        send(clientFd, userListMsg.Data().data(), userListMsg.Size(), 0);
        // 广播新用户上线
        BroadCastToAll(clientFd, msg.m_strData, YConnect, clientFd);
        break;
    }

    case YMsg:
        // 广播消息给所有客户端
        if (!msg.m_strData.empty()) {
            LOG_INFO("Broadcasting message from " + m_clientFdToIp[clientFd] + ": " + msg.m_strData);
            BroadCastToAll(clientFd, msg.m_strData, YMsg, clientFd);
        }
        break;

    case YFile:
        if (!msg.m_strData.empty()) {
            LOG_INFO("Recv a file upload msg from" + m_clientFdToIp[clientFd] + ": " + msg.m_strData);
            BroadCastToAll(clientFd, msg.m_strData, YFile, clientFd);
        }
        break;

    case YFileStart:
        HandleFileStart(clientFd, msg);
        break;

    case YFileData:
        HandleFileData(clientFd, msg);
        break;

    case YFileEnd:
        HandleFileEnd(clientFd, msg);
        break;

    case YFileAck:
        HandleFileAck(clientFd, msg);
        break;

    default:
        LOG_WARNING("Unknown message type: " + std::to_string(msg.m_sCmd));
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
            size_t packetSize = onePacket.size();
            CYondPack pack(onePacket.c_str(), packetSize);
            if (packetSize > 0) {  // 只有当包解析成功时才处理
                ProcessMessage(clientFd, pack);
            }
        });
    }
}