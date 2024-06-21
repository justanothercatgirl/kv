#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <stdio.h>

#define CONTAINER_IMPLEMENTATION
#include <container.h>
#undef CONTAINER_IMPLEMENTATION

#include "channel.h"

#define MAIN_PORT 8164

enum request_type {
	spawn_channel,
	get_channels,
};

static int* open_sockets;
static int request_socket;

void init(void) {
	open_sockets = array_new(int, 0);
	request_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(MAIN_PORT), .sin_addr = {INADDR_ANY}};
	
	for (int retries = 0; retries <= 5; ++retries) {
		if (bind(request_socket, (struct sockaddr*)&addr, sizeof(addr)) == 0) break;
		else {
			perror("init (bind)");
			sleep(1);
		}
	}
}

enum request_type wait_for_requests(void) {
	return spawn_channel;
}

int spawn_channel_thread(void) {
	return 0;	
}

void event_loop(void) {
	init();
	while (1) {
		enum request_type req = wait_for_requests();
		switch (req) {
			case spawn_channel: break;
			case get_channels: break;
		}
	}
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;
	thread_loop();
	return 0;
}
