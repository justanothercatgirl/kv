#include <netinet/in.h>
#include <stdio.h>
#include <pthread.h>

#include "../server/tcp.h"

#include <errno.h>
#include <packet.h>

#define W printf("LINE %i\n", __LINE__)

int commsock;
enum commd_error cerr;

void setup_globals(void) { commsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); }

size_t sendtoserv(int sock, enum commd_type t, struct commd *cmd, enum commd_error  * NULLABLE e) {
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in serv = {
		.sin_family = AF_INET, .sin_port = htons(TCP_PORT), .sin_addr = {htonl(INADDR_LOOPBACK)}};
	while (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) != 0) sleep(1);
	t = htonl(t);
	if (send(sock, &t, sizeof(t), 0) <= 0) { *e = ERR_SERV; return 0; }
	if (send(sock, cmd, sizeof(*cmd), 0) <= 0) { *e = ERR_SERV; return 0; }

	size_t resp;
	if (recv(sock, &resp, sizeof(resp), 0) <= 0) {*e = ERR_SERV; return 0; }
	resp = ntohzu(resp);
	if (resp == 0 && e != NULL) {
		read(sock, e, sizeof(*e));
		*e = ntohl(*e);
	}
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return resp;
}

size_t register_user(int perm) {
	struct commd_register r = {.auid = htonzu(0), .perm = htonzu(perm)};
	if (cerr != ERR_SUCCESS) DEBUGF("Server returned error %i: %s. errno: %s\n", cerr, kv_strerror(cerr), strerror(errno));
	return sendtoserv(commsock, CMD_REGISTER, (struct commd *)&r, &cerr);
}

void unreg_user(size_t u) {
	struct commd_unregister unreg = {.auid = htonzu(u), .uid = htonzu(u)};
	size_t resp = sendtoserv(commsock, CMD_UNREGISTER, (struct commd *)&unreg, &cerr);
	if (cerr != ERR_SUCCESS) DEBUGF("Server returned error %i: %s. errno: %s\n", cerr, kv_strerror(cerr), strerror(errno));
	printf("sendtoserv returned %zu\n", resp);
}

size_t create_channel(size_t u) {
	struct commd_create c = {.uid = htonzu(u)};
	size_t resp = sendtoserv(commsock, CMD_CREATE, (struct commd *)&c, &cerr);
	if (cerr != ERR_SUCCESS) DEBUGF("Server returned error %i: %s\n", cerr, kv_strerror(cerr));
	return resp;
}

void* thr(void* a) {
	(void)a;
	size_t uid = register_user(perm_none);
	printf("uid: %zu\n", uid);
	unreg_user(uid);
	uid = register_user(perm_admin);
	printf("uid: %zu\n", uid);
	size_t chid = create_channel(uid);
	printf("chid: %zu\n", chid);
	return NULL;
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;
	for (int i = 0; i < 1; ++i) {
		pthread_t t;
		pthread_create(&t, NULL, thr, NULL);
	}
	sleep(1);
	return 0;
}
