#include <cstdio>
#include <unistd.h>
#include "CChatServer.h"
#include "CYondLog.h"
#include "CYondThreadPool.h"
#include <filesystem>

int main()
{
    // 打印当前工作目录
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    }

    // 创建接收文件的目录
    try {
        std::filesystem::create_directories("received_files");
        LOG_INFO("Created received_files directory");
    } catch (const std::exception& e) {
        LOG_ERROR(YOND_ERR_OK, "Failed to create received_files directory: " + std::string(e.what()));
        return 1;
    }

    // 初始化日志系统
    if (!CYondLog::Initialize()) {
        printf("Failed to initialize logging system\n");
        return 1;
    }

    LOG_INFO("Starting chat server...");
    CYondThread StartThread;
    int err = 0;
    StartThread.Start([&err]() {
        err = CChatServer::GetInstance()->StartService();
    });
    StartThread.Detach();

    if (err != 0) {
        LOG_ERROR(err, "Service start failed");
    }
    else {
        LOG_INFO("Service started successfully");
    }

    getchar();
    StartThread.Stop();
    CChatServer::GetInstance()->StopService();

    // 关闭日志系统
    CYondLog::Shutdown();
    return 0;
}