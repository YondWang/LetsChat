#include <cstdio>
#include <sys/socket.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <filesystem>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore>
#include <pthread.h>
#include <iostream>

sem_t semid;
char name[64]{ "[default]" };

void* client_send_msg(void* arg) {
	pthread_detach(pthread_self());

	int sock = *(int*)arg;
	char msg[256]{};
	char buffer[1024]{};
	while (1) {
		memset(buffer, 0, sizeof(buffer));
		memset(msg, 0, sizeof(msg));
		fgets(msg, sizeof(msg), stdin);
		if ((strcmp(msg, "q\n") == 0) || (strcmp(msg, "Q\n") == 0)) {
			std::cout << "client exit!" << std::endl;
			break;
		}
		snprintf(buffer, sizeof(msg), "%s", msg);
		send(sock, buffer, strlen(buffer), 0);
	}
	std::cout << "stop to send" << std::endl;
	sem_post(&semid);
	pthread_exit(NULL);
}
void* client_recv_msg(void* arg) {
	pthread_detach(pthread_self());

	int sock = *(int*)arg;
	char msg[256]{};
	while (1) {
		//std::cout << "try to recving" << std::endl;
		ssize_t str_len = recv(sock, msg, sizeof(msg), 0);
		if (str_len <= 0) break;
		fputs(msg, stdout);
		memset(msg, 0, str_len);
	}
	std::cout << "stop to recv" << std::endl;
	sem_post(&semid);
	pthread_exit(NULL);
}

int startClient() {
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serv_addr.sin_port = htons(2904);
	fputs("input ur name:", stdout);
	scanf("%s", name);
	if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr))) {
		printf("connect error %d %s\r\n", errno, strerror(errno));
		return -1;
	}
	pthread_t thsnd, thrcv;

	sem_init(&semid, 0, -1);
	pthread_create(&thsnd, NULL, client_send_msg, (void*)&sock);
	pthread_create(&thrcv, NULL, client_recv_msg, (void*)&sock);
	sem_wait(&semid);
	close(sock);

	std::cout << "client exit!:" << __FUNCTION__ << std::endl;

	return 0;
}

void test() {
	std::string str = "FILE test.txt 1024";
	std::string filename;
	std::string filesize_str;

}

int main()
{
	startClient();

	//test();

	return 0;
}