#include <netinet/in.h>
#include <opus/opus.h>

#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../server/channel.h"

#define CONTAINER_IMPLEMENTATION
#include <container.h>
#undef CONTAINER_IMPLEMENTATION

void setup_socket(void)
{
	int		   serv_sock;
	struct sockaddr_in serv_addr = {.sin_family = AF_INET,
					.sin_port   = htons(8082),
					.sin_addr   = {0}};
	inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
	if ((serv_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	if (connect(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
		perror("connect");
		exit(EXIT_FAILURE);
	}
	const char* buf = "Hello, world!\n";
	size_t buflen = strlen(buf);
	if (send(serv_sock, buf, buflen, 0) < 0) {
		perror("send");
		exit(EXIT_FAILURE);
	}
	puts("sent!");
}

void test_channel(void) {
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	struct sockaddr_in client = {
		.sin_family = AF_INET,
		.sin_port = 8080,
		.sin_addr = {INADDR_ANY}
	};

	(void)bind(sock, (struct sockaddr*)&client, sizeof(client));
	struct sockaddr_in serv = {
		.sin_family = AF_INET,
		.sin_port = 6969
	};
	inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);
	struct kv_system_packet req = {
		.operation_id = join_channel,
		.return_port = 8080,
		.return_address = htonl((127 << 24) | 1),
		.user_id = 69,
	}, resp;
	req.checksum = system_packet_checksum(&req);
	sendto(sock, &req, KV_PACKET_SIZE, 0, (struct sockaddr*)&serv, sizeof(serv));
	recvfrom(sock, &resp, KV_PACKET_SIZE, 0, NULL, NULL);
	if (resp.operation_id == acknowledgement) {
		puts("УРА УРА МЕНЯ ПУСТИЛИ");
	} else {
		puts("О нет, что же пошло не так..");
	}
	
	int pid = fork();
	if (pid == 0) {
		sleep(2);
		puts("child activated");
		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		struct sockaddr_in client = {
			.sin_family = AF_INET,
			.sin_port = 8081,
			.sin_addr = {INADDR_ANY}
		};
		(void)bind(sock, (struct sockaddr*)&client, sizeof(client));
		req.user_id = 80809;
		req.return_port = 8081;
		req.checksum = system_packet_checksum(&req);
		sendto(sock, &req, KV_PACKET_SIZE, 0, (struct sockaddr*)&serv, sizeof(serv));
		recvfrom(sock, &resp, KV_PACKET_SIZE, 0, NULL, NULL);
		if (resp.operation_id == acknowledgement) {
			puts("УРА УРА МЕНЯ ПУСТИЛИ (child)");
			struct kv_packet message;
			message.id = 70;
			int i = 0;
			for (const char * x = "ПРИВЕЕЕТ :D\n\0"; *x; ++x) {
				message.data[i] = *x;
				++i; 
				message.data[i] = 0;
			}
			message.data[100] = 0;
			sendto(sock, &message, KV_PACKET_SIZE, 0, (struct sockaddr*)&serv, sizeof(serv));
		} else {
			puts("О нет, что же пошло не так.. (child)");
		}

	} else {
		memset(&req, 0, KV_PACKET_SIZE);
		recvfrom(sock, &req, KV_PACKET_SIZE, 0, NULL, NULL);
		struct kv_packet *fromchild = (struct kv_packet*)&req;
		printf("req.id = %i, req.data = %s (parent recv)\n", fromchild->id, fromchild->data);
		wait(NULL);
	}

}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	test_channel();
	return 0;
}
