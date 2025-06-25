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
        //HandleFileStart(clientFd, msg);

        break;

    case YFileStart:
        m_fileTransferThreadPool.Enqueue([this, clientFd, msg]() {
            HandleFileStart(clientFd, msg);
        });
        break;

    case YFileData:
        m_fileTransferThreadPool.Enqueue([this, clientFd, msg]() {
            HandleFileData(clientFd, msg);
        });
        break;

    case YFileEnd:
        m_fileTransferThreadPool.Enqueue([this, clientFd, msg]() {
            HandleFileEnd(clientFd, msg);
        });
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
        if (buffer.size() < 10) break;
        // 查找包头
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
            // 没有包头，全部数据都属于上一个包的残留
            if (m_fileTransfers.count(clientFd) && !buffer.empty()) {
                FileTransferState& state = m_fileTransfers[clientFd];
                state.receivedPackets.push(buffer);
                state.receivedSize += buffer.size();
            }
            buffer.clear();
            break;
        }
        if (headerPos > 0) {
            // FEFF前的数据属于上一个包的残留
            if (m_fileTransfers.count(clientFd)) {
                FileTransferState& state = m_fileTransfers[clientFd];
                std::string remain = buffer.substr(0, headerPos);
                state.receivedPackets.push(remain);
                state.receivedSize += remain.size();
            }
            buffer.erase(0, headerPos);
        }
        if (buffer.size() < 8) break;
        uint32_t length;
        memcpy(&length, buffer.data() + 2, 4);
        length = ntohl(length);
        size_t totalLen = 2 + 4 + length + 2;
        if (buffer.size() < totalLen) break;
        std::string onePacket = buffer.substr(0, totalLen);
        buffer.erase(0, totalLen);
        m_threadPool.Enqueue([this, clientFd, onePacket]() {
            size_t packetSize = onePacket.size();
            CYondPack pack(onePacket.c_str(), packetSize);
            if (packetSize > 0) {
                ProcessMessage(clientFd, pack);
            }
        });
    }
}

void CYondHandleEvent::HandleFileEnd(int clientFd, const CYondPack& msg) {
    std::lock_guard<std::mutex> lock(m_fileTransfersMutex);
    auto it = m_fileTransfers.find(clientFd);
    if (it == m_fileTransfers.end()) {
        LOG_ERROR(YOND_ERR_RECV_PACKET, "No active file transfer for client");
        return;
    }

    FileTransferState& state = it->second;
    std::unique_lock<std::mutex> stateLock(*state.mutex);

    // 等待所有数据包接收完成
    state.cv->wait(stateLock, [&state] { return state.isComplete; });

    // 按顺序写入文件，只写totalSize字节
    std::string filePath = "received_files/" + state.fileName;
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR(YOND_ERR_RECV_PACKET, "Failed to open file for writing: " + filePath);
        return;
    }

    size_t written = 0;
    while (!state.receivedPackets.empty() && written < state.totalSize) {
        std::string packet = state.receivedPackets.front();
        size_t toWrite = std::min(packet.size(), state.totalSize - written);
        file.write(packet.c_str(), toWrite);
        written += toWrite;
        state.receivedPackets.pop();
    }

    file.close();

    // 发送完成确认
    CYondPack ackMsg(YFileAck, msg.m_sUser, "END", 3);
    send(clientFd, ackMsg.Data().data(), ackMsg.Size(), 0);

    // 清理传输状态
    m_fileTransfers.erase(it);
}

void CYondHandleEvent::HandleFileData(int clientFd, const CYondPack& msg) {
    std::lock_guard<std::mutex> lock(m_fileTransfersMutex);
    auto it = m_fileTransfers.find(clientFd);
    if (it == m_fileTransfers.end()) {
        LOG_ERROR(YOND_ERR_RECV_PACKET, "No active file transfer for client");
        return;
    }

    FileTransferState& state = it->second;
    std::lock_guard<std::mutex> stateLock(*state.mutex);

    // 判断是否超出totalSize
    size_t remain = state.totalSize > state.receivedSize ? state.totalSize - state.receivedSize : 0;
    if (remain == 0) {
        // 已经收满，忽略多余包
        return;
    }
    if (msg.m_strData.size() > remain) {
        // 只取需要的部分
        state.receivedPackets.push(msg.m_strData.substr(0, remain));
        state.receivedSize += remain;
    } else {
        state.receivedPackets.push(msg.m_strData);
        state.receivedSize += msg.m_strData.size();
    }

    // 发送确认消息
    CYondPack ackMsg(YFileAck, msg.m_sUser, "DATA", 4);
    send(clientFd, ackMsg.Data().data(), ackMsg.Size(), 0);

    // 检查是否接收完成
    if (state.receivedSize >= state.totalSize) {
        state.isComplete = true;
        state.cv->notify_all();
        LOG_INFO("recieve complete!!");
        return;
    }
    // 进度日志，不是错误
    LOG_INFO("file receiving... recv size: " + std::to_string(state.receivedSize) + " total size: " + std::to_string(state.totalSize));
}