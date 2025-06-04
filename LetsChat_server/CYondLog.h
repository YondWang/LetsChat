#pragma once
#include <stdio.h>
#include <string>

//return CYondLog::log(CYondLog::LOG_LEVEL_ERROR, __FILE__, __LINE__, YOND_ERR_SOCKET_CREATE, "socket init error");


class CYondLog
{
public:
	enum log_level {
		LOG_LEVEL_INFO,
		LOG_LEVEL_WARNING,
		LOG_LEVEL_ERROR
	};
	static int log(int level, const char* file, int line, int err, std::string msg) {
		if (level == LOG_LEVEL_INFO) {
			printf("%s:%d MESSAGE:[%s] ", file, line, msg.c_str());
			return err;
		}
		else if (level == LOG_LEVEL_WARNING) {
			printf("WARNING: ");
			return err;
		}
		else if (level == LOG_LEVEL_ERROR) {
			/*printf("ERROR: ");
			switch (err) {
			case 0:
				printf("No error\n");
				break;
			case 2000:
				printf("Error creating socket\n");
				break;
			case 2001:
				printf("Error binding socket\n");
				break;
			case 2002:
				printf("Error listening on socket\n");
				break;
			case 2003:
				printf("Error accepting connection\n");
				break;
			case 2004:
				printf("Error sending data\n");
				break;
			case 2005:
				printf("Error receiving data\n");
				break;
			case 2006:
				printf("Error creating epoll instance\n");
				break;
			default:
				printf("Unknown error code: %d\n", err);
				break;
			}*/
			return error_log(file, line, err, msg);
		}
		else {
			printf("Unknown log level\n");
			return err; // Unknown log level
		}
	}

	static int error_log(const char* file, int line, int err, std::string msg) {
		printf("%s:%d ERROR:[%d] MESSAGE:[%s] ", file, line, err, msg.c_str());
		return err;
	}
		
};