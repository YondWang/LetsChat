#include "CYondHandleEvent.h"
#include <unistd.h>
#include <linux/limits.h>
#include <filesystem>

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

    case YRecv: {
        StartFileDownload(clientFd, msg.m_strData);
        break;
    }
    case YFileAck: {
        AdvanceFileDownload(clientFd, msg);
        break;
    }

    default:
        LOG_WARNING("Unknown message type: " + std::to_string(msg.m_sCmd));
    }
}

void CYondHandleEvent::StartFileDownload(int clientFd, const std::string& filename) {
    char exePath[PATH_MAX] = { 0 };
    ssize_t count = readlink("/proc/self/exe", exePath, PATH_MAX);
    std::string exeDir;
    if (count != -1) {
        exePath[count] = '\0';
        exeDir = std::filesystem::path(exePath).parent_path().string();
    }
    else {
        exeDir = ".";
    }
    std::string filepath = exeDir + "/received_files/" + filename;
    LOG_INFO("[YRecv] filename=" + filename + ", filepath=" + filepath);
    std::string hex;
    for (size_t i = 0; i < filename.size(); ++i) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02X ", (unsigned char)filename[i]);
        hex += buf;
    }
    LOG_INFO("[YRecv] filename hex=" + hex);
    std::ifstream* file = new std::ifstream(filepath, std::ios::binary | std::ios::ate);
    if (!file->is_open()) {
        LOG_ERROR(YOND_ERR_FILE_RECV, "[YRecv] File not found: " + filepath + ", errno=" + std::to_string(errno) + ", " + strerror(errno));
        std::string errMsg = "FILE_NOT_FOUND ";
        CYondPack errPack(YFile, clientFd, errMsg.c_str(), errMsg.size());
        send(clientFd, errPack.Data().data(), errPack.Size(), 0);
        delete file;
        return;
    }
    size_t filesize = file->tellg();
    file->seekg(0, std::ios::beg);

    m_downloadStates[clientFd] = DownloadState{ file, filename, filesize, 0, 0, 0 };
    // 1. 发送YFileStart
    std::string fileStartData = filename + "|" + std::to_string(filesize);
    CYondPack fileStartPack(YFileStart, clientFd, fileStartData.c_str(), fileStartData.size());
    send(clientFd, fileStartPack.Data().data(), fileStartPack.Size(), 0);
    LOG_INFO("[YRecv] Send YFileStart, wait for ACK(START)...");
    // 不再阻塞recv，等主线程收到ACK后推进
}

void CYondHandleEvent::AdvanceFileDownload(int clientFd, const CYondPack& msg) {
    auto it = m_downloadStates.find(clientFd);
    if (it == m_downloadStates.end()) {
        HandleFileAck(clientFd, msg); // 兼容上传
        return;
    }
    auto& state = it->second;
    LOG_INFO("[YRecv] AdvanceFileDownload: step=" + std::to_string(state.step) +
        ", sent=" + std::to_string(state.sent) +
        ", filesize=" + std::to_string(state.filesize) +
        ", dataIdx=" + std::to_string(state.dataIdx) +
        ", ackType=" + msg.m_strData);
    if (msg.m_strData == "START" && state.step == 0) {
        const size_t BUF_SIZE = 8192;
        char buf[BUF_SIZE];
        size_t toRead = std::min(BUF_SIZE, state.filesize - state.sent);
        state.file->read(buf, toRead);
        size_t actuallyRead = state.file->gcount();
        LOG_INFO("[YRecv] file->read: toRead=" + std::to_string(toRead) + ", actuallyRead=" + std::to_string(actuallyRead));
        if (actuallyRead == 0) {
            LOG_ERROR(ERR_LOG_DOWNLOAD_FILE, "[YRecv] file.gcount()==0, break");
            state.file->close();
            delete state.file;
            m_downloadStates.erase(it);
            return;
        }
        LOG_INFO("[YRecv] Send YFileData idx=" + std::to_string(state.dataIdx) + ", sent=" + std::to_string(state.sent) + ", toRead=" + std::to_string(toRead));
        CYondPack dataPack(YFileData, clientFd, buf, actuallyRead);
        send(clientFd, dataPack.Data().data(), dataPack.Size(), 0);
        state.sent += actuallyRead;
        state.dataIdx++;
        state.step = 1;
    }
    else if (msg.m_strData == "DATA" && state.step == 1) {
        LOG_INFO("[YRecv] DATA分支: state.sent=" + std::to_string(state.sent) +
            ", state.filesize=" + std::to_string(state.filesize) +
            ", dataIdx=" + std::to_string(state.dataIdx));
        if (state.sent < state.filesize) {
            const size_t BUF_SIZE = 8192;
            char buf[BUF_SIZE];
            size_t toRead = std::min(BUF_SIZE, state.filesize - state.sent);
            state.file->read(buf, toRead);
            size_t actuallyRead = state.file->gcount();
            LOG_INFO("[YRecv] file->read: toRead=" + std::to_string(toRead) + ", actuallyRead=" + std::to_string(actuallyRead));
            if (actuallyRead == 0) {
                LOG_ERROR(ERR_LOG_DOWNLOAD_FILE, "[YRecv] file.gcount()==0, break");
                state.file->close();
                delete state.file;
                m_downloadStates.erase(it);
                return;
            }
            LOG_INFO("[YRecv] Send YFileData idx=" + std::to_string(state.dataIdx) + ", sent=" + std::to_string(state.sent) + ", toRead=" + std::to_string(toRead));
            CYondPack dataPack(YFileData, clientFd, buf, actuallyRead);
            send(clientFd, dataPack.Data().data(), dataPack.Size(), 0);
            state.sent += actuallyRead;
            state.dataIdx++;
        }
        else {
            LOG_INFO("[YRecv] DATA分支结束: state.sent=" + std::to_string(state.sent) +
                ", state.filesize=" + std::to_string(state.filesize) +
                ", dataIdx=" + std::to_string(state.dataIdx));
            CYondPack fileEndPack(YFileEnd, clientFd, state.filename.c_str(), state.filename.size());
            send(clientFd, fileEndPack.Data().data(), fileEndPack.Size(), 0);
            LOG_INFO("[YRecv] All data sent, send YFileEnd, wait for ACK(END)...");
            state.step = 2;
        }
    }
    else if (msg.m_strData == "END" && state.step == 2) {
        // 传输完成，清理状态
        state.file->close();
        delete state.file;
        m_downloadStates.erase(it);
        LOG_INFO("[YRecv] File sent to client (ACK驱动): " + msg.m_strData);
    }
    else {
        // 兼容上传
        HandleFileAck(clientFd, msg);
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
        LOG_INFO("[ExtractAndProcessPackets] Got onePacket, size=" + std::to_string(onePacket.size()));
        CYondPack pack(onePacket.c_str(), onePacket.size());
        if (pack.m_sCmd == YFileAck || pack.m_sCmd == YRecv) {
            // 下载相关包，主线程直接处理，保证顺序
            LOG_INFO("[ProcessMessage] (direct) cmd=" + std::to_string(pack.m_sCmd) + ", data=" + pack.m_strData);
            ProcessMessage(clientFd, pack);
        }
        else {
            m_threadPool.Enqueue([this, clientFd, pack]() {
                LOG_INFO("[ProcessMessage] (thread) cmd=" + std::to_string(pack.m_sCmd) + ", data=" + pack.m_strData);
                ProcessMessage(clientFd, pack);
                });
        }
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

void CYondHandleEvent::HandleFileDownload(int clientFd, const std::string& filename) {
    // 直接调用下载状态机入口
    StartFileDownload(clientFd, filename);
}

void CYondHandleEvent::HandleFileAck(int clientFd, const CYondPack& msg) {
    std::lock_guard<std::mutex> lock(m_fileTransfersMutex);
    auto it = m_fileTransfers.find(clientFd);
    if (it == m_fileTransfers.end()) {
        LOG_WARNING("[HandleFileAck] No active upload state for client " + std::to_string(clientFd));
        return;
    }
    FileTransferState& state = it->second;
    if (msg.m_strData == "START") {
        // 客户端收到YFileStart后回ACK(START)，可做准备
        LOG_INFO("[HandleFileAck] Upload: client " + std::to_string(clientFd) + " confirmed START");
        // 可选：设置状态
    } else if (msg.m_strData == "DATA") {
        // 客户端收到YFileData后回ACK(DATA)，可做进度统计
        LOG_INFO("[HandleFileAck] Upload: client " + std::to_string(clientFd) + " confirmed DATA, progress: " + std::to_string(state.receivedSize) + "/" + std::to_string(state.totalSize));
        // 可选：统计进度、重传等
    } else if (msg.m_strData == "END") {
        // 客户端收到YFileEnd后回ACK(END)，可做收尾
        LOG_INFO("[HandleFileAck] Upload: client " + std::to_string(clientFd) + " confirmed END, upload complete");
        m_fileTransfers.erase(it);
    } else {
        LOG_WARNING("[HandleFileAck] Unknown ACK type: " + msg.m_strData);
    }
}