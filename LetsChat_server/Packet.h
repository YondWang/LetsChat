#pragma once
#include <string>
#include <vector>
#include <cstdint>

// 消息类型定义
enum class PacketType : uint16_t {
    CHAT = 1,           // 聊天消息
    FILE_START = 2,     // 文件传输开始
    FILE_DATA = 3,      // 文件数据
    FILE_END = 4,       // 文件传输结束
    SYSTEM = 5          // 系统消息
};

// 包头结构
#pragma pack(push, 1)
struct PacketHeader {
    uint32_t magic;         // 魔数：0x12345678
    uint16_t type;          // 消息类型
    uint32_t totalSize;     // 总大小（包括包头）
    uint32_t seqNum;        // 序列号（用于文件分片）
    uint32_t dataSize;      // 数据大小
};
#pragma pack(pop)

class Packet {
public:
    enum class Type : uint16_t {
        ChatMessage = 1,
        FileStart = 2,
        FileData = 3,
        FileEnd = 4,
        SystemMessage = 5
    };

    static constexpr uint32_t MAGIC_NUMBER = 0x12345678;
    static constexpr uint32_t MAX_DATA_SIZE = 8192;  // 8KB per packet

    struct PacketHeader {
        uint32_t magic;
        uint16_t type;
        uint32_t totalSize;
        uint32_t sequence;
        uint32_t dataSize;
    } __attribute__((packed));

    // 构造函数和析构函数
    Packet();
    ~Packet();
    Packet(const Packet& other);
    Packet& operator=(const Packet& other);

    // 静态创建函数
    static Packet createChatMessage(const std::string& sender, const std::string& content);
    static Packet createFileStart(const std::string& fileName, uint32_t fileSize);
    static Packet createFileData(uint32_t sequence, const char* data, uint32_t size);
    static Packet createFileEnd();
    static Packet createSystemMessage(const std::string& content);

    // 数据包解析
    bool parse(const char* data, uint32_t size);

    // 数据获取函数
    void getChatMessage(std::string& sender, std::string& content) const;
    void getFileStart(std::string& fileName, uint32_t& fileSize) const;
    void getFileData(const char*& data, uint32_t& size) const;
    void getSystemMessage(std::string& content) const;

    // 获取数据包信息
    Type getType() const { return static_cast<Type>(m_header.type); }
    uint32_t getTotalSize() const { return m_header.totalSize; }
    uint32_t getDataSize() const { return m_header.dataSize; }
    const char* getData() const { return m_data; }
    const PacketHeader* getHeader() const { return &m_header; }
    uint32_t getHeaderSize() const { return sizeof(PacketHeader); }

    // 从原始数据解析包
    static bool parsePacket(const std::string& rawData, Packet& packet) {
        if (rawData.size() < sizeof(PacketHeader)) {
            return false;
        }

        const PacketHeader* header = reinterpret_cast<const PacketHeader*>(rawData.data());
        if (header->magic != MAGIC_NUMBER) {
            return false;
        }

        if (header->totalSize > MAX_DATA_SIZE) {
            return false;
        }

        packet.m_header.magic = MAGIC_NUMBER;
        packet.m_header.type = header->type;
        packet.m_header.totalSize = sizeof(PacketHeader) + header->dataSize;
        packet.m_header.sequence = header->seqNum;
        packet.m_header.dataSize = header->dataSize;
        packet.m_data = const_cast<char*>(rawData.substr(sizeof(PacketHeader), header->dataSize).c_str());
        return true;
    }

    // 将文件数据分片
    static std::vector<Packet> splitFileData(const std::vector<uint8_t>& fileData) {
        std::vector<Packet> packets;
        uint32_t seqNum = 0;
        size_t offset = 0;

        while (offset < fileData.size()) {
            size_t chunkSize = std::min(MAX_DATA_SIZE, static_cast<uint32_t>(fileData.size() - offset));
            std::vector<uint8_t> chunk(fileData.begin() + offset, fileData.begin() + offset + chunkSize);
            packets.push_back(createFileData(seqNum++, chunk.data(), chunk.size()));
            offset += chunkSize;
        }

        return packets;
    }

private:
    PacketHeader m_header;
    char* m_data;
}; 