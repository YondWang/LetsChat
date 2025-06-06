#include "Packet.h"
#include <cstring>

Packet::Packet() : m_data(nullptr) {
    memset(&m_header, 0, sizeof(PacketHeader));
}

Packet::~Packet() {
    delete[] m_data;
}

Packet::Packet(const Packet& other) : m_data(nullptr) {
    m_header = other.m_header;
    if (other.m_data && other.m_header.dataSize > 0) {
        m_data = new char[other.m_header.dataSize];
        memcpy(m_data, other.m_data, other.m_header.dataSize);
    }
}

Packet& Packet::operator=(const Packet& other) {
    if (this != &other) {
        delete[] m_data;
        m_header = other.m_header;
        m_data = nullptr;
        if (other.m_data && other.m_header.dataSize > 0) {
            m_data = new char[other.m_header.dataSize];
            memcpy(m_data, other.m_data, other.m_header.dataSize);
        }
    }
    return *this;
}

Packet Packet::createChatMessage(const std::string& sender, const std::string& content) {
    Packet packet;
    packet.m_header.magic = MAGIC_NUMBER;
    packet.m_header.type = static_cast<uint16_t>(Type::ChatMessage);
    packet.m_header.sequence = 0;
    
    // 计算总大小
    uint32_t totalSize = sizeof(PacketHeader);
    totalSize += sender.length() + 1;  // +1 for null terminator
    totalSize += content.length() + 1;  // +1 for null terminator
    
    packet.m_header.totalSize = totalSize;
    packet.m_header.dataSize = totalSize - sizeof(PacketHeader);
    
    // 分配数据缓冲区
    packet.m_data = new char[packet.m_header.dataSize];
    char* ptr = packet.m_data;
    
    // 复制发送者
    strcpy(ptr, sender.c_str());
    ptr += sender.length() + 1;
    
    // 复制内容
    strcpy(ptr, content.c_str());
    
    return packet;
}

Packet Packet::createFileStart(const std::string& fileName, uint32_t fileSize) {
    Packet packet;
    packet.m_header.magic = MAGIC_NUMBER;
    packet.m_header.type = static_cast<uint16_t>(Type::FileStart);
    packet.m_header.sequence = 0;
    
    // 计算总大小
    uint32_t totalSize = sizeof(PacketHeader);
    totalSize += fileName.length() + 1;  // +1 for null terminator
    totalSize += sizeof(uint32_t);  // fileSize
    
    packet.m_header.totalSize = totalSize;
    packet.m_header.dataSize = totalSize - sizeof(PacketHeader);
    
    // 分配数据缓冲区
    packet.m_data = new char[packet.m_header.dataSize];
    char* ptr = packet.m_data;
    
    // 复制文件名
    strcpy(ptr, fileName.c_str());
    ptr += fileName.length() + 1;
    
    // 复制文件大小
    memcpy(ptr, &fileSize, sizeof(uint32_t));
    
    return packet;
}

Packet Packet::createFileData(uint32_t sequence, const char* data, uint32_t size) {
    Packet packet;
    packet.m_header.magic = MAGIC_NUMBER;
    packet.m_header.type = static_cast<uint16_t>(Type::FileData);
    packet.m_header.sequence = sequence;
    
    // 计算总大小
    uint32_t totalSize = sizeof(PacketHeader);
    totalSize += size;
    
    packet.m_header.totalSize = totalSize;
    packet.m_header.dataSize = size;
    
    // 分配数据缓冲区
    packet.m_data = new char[size];
    memcpy(packet.m_data, data, size);
    
    return packet;
}

Packet Packet::createFileEnd() {
    Packet packet;
    packet.m_header.magic = MAGIC_NUMBER;
    packet.m_header.type = static_cast<uint16_t>(Type::FileEnd);
    packet.m_header.sequence = 0;
    packet.m_header.totalSize = sizeof(PacketHeader);
    packet.m_header.dataSize = 0;
    packet.m_data = nullptr;
    return packet;
}

Packet Packet::createSystemMessage(const std::string& content) {
    Packet packet;
    packet.m_header.magic = MAGIC_NUMBER;
    packet.m_header.type = static_cast<uint16_t>(Type::SystemMessage);
    packet.m_header.sequence = 0;
    
    // 计算总大小
    uint32_t totalSize = sizeof(PacketHeader);
    totalSize += content.length() + 1;  // +1 for null terminator
    
    packet.m_header.totalSize = totalSize;
    packet.m_header.dataSize = totalSize - sizeof(PacketHeader);
    
    // 分配数据缓冲区
    packet.m_data = new char[packet.m_header.dataSize];
    strcpy(packet.m_data, content.c_str());
    
    return packet;
}

bool Packet::parse(const char* data, uint32_t size) {
    if (size < sizeof(PacketHeader)) {
        return false;
    }
    
    // 复制头部
    memcpy(&m_header, data, sizeof(PacketHeader));
    
    // 验证魔数
    if (m_header.magic != MAGIC_NUMBER) {
        return false;
    }
    
    // 验证总大小
    if (size != m_header.totalSize) {
        return false;
    }
    
    // 分配并复制数据
    if (m_header.dataSize > 0) {
        m_data = new char[m_header.dataSize];
        memcpy(m_data, data + sizeof(PacketHeader), m_header.dataSize);
    } else {
        m_data = nullptr;
    }
    
    return true;
}

void Packet::getChatMessage(std::string& sender, std::string& content) const {
    if (m_header.type != static_cast<uint16_t>(Type::ChatMessage) || !m_data) {
        return;
    }
    
    const char* ptr = m_data;
    sender = ptr;
    ptr += sender.length() + 1;
    content = ptr;
}

void Packet::getFileStart(std::string& fileName, uint32_t& fileSize) const {
    if (m_header.type != static_cast<uint16_t>(Type::FileStart) || !m_data) {
        return;
    }
    
    const char* ptr = m_data;
    fileName = ptr;
    ptr += fileName.length() + 1;
    memcpy(&fileSize, ptr, sizeof(uint32_t));
}

void Packet::getFileData(const char*& data, uint32_t& size) const {
    if (m_header.type != static_cast<uint16_t>(Type::FileData) || !m_data) {
        data = nullptr;
        size = 0;
        return;
    }
    
    data = m_data;
    size = m_header.dataSize;
}

void Packet::getSystemMessage(std::string& content) const {
    if (m_header.type != static_cast<uint16_t>(Type::SystemMessage) || !m_data) {
        return;
    }
    
    content = m_data;
} 