#pragma once
#include <stdio.h>
class CYondLog
{
	enum log_level {
		LOG_LEVEL_INFO,
		LOG_LEVEL_WARNING,
		LOG_LEVEL_ERROR
	};
	static void log(int level, int err) {
		if (level == LOG_LEVEL_INFO) {
			printf("INFO: ");
		}
		else if (level == LOG_LEVEL_WARNING) {
			printf("WARNING: ");
		}
		else if (level == LOG_LEVEL_ERROR) {
			printf("ERROR: ");
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
			}
		}
		else {
			printf("Unknown log level\n");
		}
	}
		
};