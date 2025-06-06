#include "CYondHandleEvent.h"
#include "CYondLog.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <iostream>

int main() {
    // 初始化日志系统
    if (!CYondLog::Initialize("logs")) {
        std::cerr << "Failed to initialize logging system" << std::endl;
        return 1;
    }
    LOG_INFO("Chat server starting...");

    // 创建服务器socket
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        LOG_ERROR(YOND_ERR_SOCKET_CREATE, "Failed to create socket");
        return 1;
    }

    // 设置socket选项
    int opt = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR(YOND_ERR_SOCKET_OPT, "Failed to set socket options");
        return 1;
    }

    // 绑定地址
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        LOG_ERROR(YOND_ERR_SOCKET_BIND, "Failed to bind socket");
        return 1;
    }

    // 开始监听
    if (listen(serverFd, 10) < 0) {
        LOG_ERROR(YOND_ERR_SOCKET_LISTEN, "Failed to listen on socket");
        return 1;
    }

    LOG_INFO("Server listening on port 8080");

    // 创建事件处理器
    CYondHandleEvent eventHandler;
    if (!eventHandler.Initialize()) {
        LOG_ERROR(YOND_ERR_EPOLL_CREATE, "Failed to initialize event handler");
        return 1;
    }

    // 主循环
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientLen);

        if (clientFd < 0) {
            LOG_ERROR(YOND_ERR_SOCKET_ACCEPT, "Failed to accept connection");
            continue;
        }

        std::string clientIp = inet_ntoa(clientAddr.sin_addr);
        if (!eventHandler.addNew(clientFd, clientIp)) {
            LOG_ERROR(YOND_ERR_EPOLL_CTL, "Failed to add new client");
            close(clientFd);
            continue;
        }
    }

    // 清理
    close(serverFd);
    CYondLog::Shutdown();
    return 0;
}