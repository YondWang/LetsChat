#include <cstdio>
#include "CChatServer.h"
#include "CYondLog.h"

int main()
{
    CChatServer chatServer;
    int err = chatServer.GetInstance()->StartService();
    if (err != 0) {
        CYondLog::log(CYondLog::LOG_LEVEL_ERROR, __FILE__, __LINE__, err, "err occ");
    }
    else {
		CYondLog::log(CYondLog::LOG_LEVEL_INFO, __FILE__, __LINE__, 0, "Service started successfully");
	}

    getchar();
    return 0;
}