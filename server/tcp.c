#include "tcp.h"
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>

#define TCP_MAX_WAIT_MS 10
#define TCP_MAX_RETRIES 0
#define ADMIN_UID	0

#define report_error(socket, error)                                                                                    \
	do {                                                                                                           \
		DEBUGF("error on socket %i: %i\n", socket, error);                                                     \
		write(socket, &zerozu, sizeof(zerozu));                                                                \
		enum commd_error __ecpy = htonl(error);                                                                \
		write(socket, &__ecpy, sizeof(__ecpy));                                                                \
		return;                                                                                                \
	} while (0)

#define return_error(err)                                                                                              \
	do {                                                                                                           \
		*e = err;                                                                                              \
		return 0;                                                                                              \
	} while (0)

struct hash_set channels;
struct hash_set users;
static struct tcp_user varusr = {0};
static struct tcp_channel varchnl = {0};
static size_t lalloc_usr = 0;
static int udpctlfd = 0;
static const size_t zerozu = 0;
static bool should_exit = false;

void print_state(int _) {
	(void)_;
#ifdef DEBUG
	fputs("printing server state.\n hash_map<struct tcp_user> users {\n", stderr);
	struct hash_set_iter iter;
	for (hseti_begin(&users, &iter); !hseti_end(&iter); hseti_next(&iter)) {
		struct tcp_user *curr = hseti_get(&iter);
		fprintf(stderr, "\ttcp_user {.id=%zu, .permissions=%u, .channel=%zu}\n", curr->id, curr->permissions,
			curr->joined_channel);
	}
	fputs("}\nhash_map<struct tcp_channel> channels {\n", stderr);
	for (hseti_begin(&channels, &iter); !hseti_end(&iter); hseti_next(&iter)) {
		struct tcp_channel *curr = hseti_get(&iter);
		struct sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);
		getsockname(curr->fd, (struct sockaddr *)&addr, &addrlen);
		fprintf(stderr, "\ttcp_channel {.id=%zu, .fd=%u, .channel=%zu} [port=%hu]\n", curr->id, curr->fd,
			curr->owner, addr.sin_port);
	}
	fputs("}\n", stderr);
#endif
}
void exit_tcp(int _) {
	(void)_;
	should_exit = true;
	DEBUGF("EXITING SERVER, setting `should_exit (%p)` to %i\n", (void*)&should_exit, (int)should_exit);
}

static int tcp_user_cmp(const struct tcp_user *a, const struct tcp_user *b) { return (a->id - b->id) ? 1 : 0; }
static int tcp_channel_cmp(const struct tcp_channel *a, const struct tcp_channel *b) { return (a->id - b->id) ? 1 : 0; }
static size_t tcp_user_hash(const struct tcp_user *a) { return a->id; }
static size_t tcp_channel_hash(const struct tcp_channel *a) { return a->id; }
static int set_sock_timeout(int fd, int ms) {
	struct timeval timeout;
	timeout.tv_sec = ms / 1000;
	timeout.tv_usec = (ms % 1000) * 1000;
	return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}
static void init_statics(void) {
	channels =
		hset_new(sizeof(struct tcp_channel), (hset_equal_fn)&tcp_channel_cmp, (hset_hash_fn)&tcp_channel_hash);
	users = hset_new(sizeof(struct tcp_user), (hset_equal_fn)&tcp_user_cmp, (hset_hash_fn)&tcp_user_hash);
	udpctlfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}
static int setup_socket(unsigned short port) {
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in localaddr = {
		.sin_family = AF_INET, .sin_port = htons(port), .sin_addr = {htonl(INADDR_ANY)}};
	if (bind(sock, (struct sockaddr *)&localaddr, sizeof(localaddr)) != 0) {
		perror("TCP thread failed to initialize");
		exit(EXIT_FAILURE);
	}
	return sock;
}
static void init_admin(size_t aid) {
	struct tcp_user u = {.id = aid, .pubkey = NULL, .permissions = perm_admin, .joined_channel = 0};
	hset_insert_copy(&users, &u);
}
static bool has_4bytes_0xff(size_t id) {
	return (unsigned int)(id >> 32) == 0xFFFFFFFF || (unsigned int)(id & 0xFFFFFFFF) == 0xFFFFFFFF;
}
static size_t get_uniq_id(struct hash_set *set) {
	// while (map has lalloc_usr inex || lalloc_usr has 4 bytes of ones) next lalloc_usr;
	varusr.id = lalloc_usr;
	while (hset_at(set, &varusr) != NULL || has_4bytes_0xff(lalloc_usr)) ++varusr.id;
	lalloc_usr = varusr.id;
	return lalloc_usr;
}
static unsigned short get_channel_port(pthread_t id) {
	varchnl.id = id;
	struct tcp_channel *ch = hset_at(&channels, &varchnl);
	if (ch == NULL) return 0;
	struct sockaddr_in a;
	socklen_t len = sizeof a;
	getsockname(ch->fd, (struct sockaddr *)&a, &len);
	return a.sin_port;
}
static bool user_has_permission(size_t uid, unsigned int perm) {
	varusr.id = uid;
	struct tcp_user *u = hset_at(&users, &varusr);
	if (u == NULL) return false;
	unsigned int uperm = u->permissions;
	// bitwise implication must yield all ones (0xFFFFFFFF).
	// Inverse it for easier check
	return (perm & ~uperm) == 0;
}
static size_t send_channels(int sockfd, enum commd_error *e) {
	struct hash_set_iter iter;
	size_t array_length = htonzu(hset_count(&channels));
	if (write(sockfd, &array_length, sizeof(array_length)) != sizeof(array_length)) return_error(ERR_SERV);
	for (hseti_begin(&channels, &iter); !hseti_end(&iter); hseti_next(&iter)) {
		struct tcp_channel *c = hseti_get(&iter);
		pthread_t chid = htonzu(c->id);
		if (write(sockfd, &chid, sizeof(chid)) != sizeof(chid)) return_error(ERR_SERV);
	}
	// the leading zero is written by the caller
	return_error(ERR_SUCCESS); // actually returns success but...
}
static inline size_t commd_register_process(struct commd_register *cmd, enum commd_error *e) {
	/* fprintf(stderr, "%s: auid=%zu; perm=%zu\n", "commd_register_process", cmd->auid, cmd->perm); */
	if (!user_has_permission(cmd->auid, perm_join_user | cmd->perm)) return_error(ERR_ACCESS);
	struct tcp_user new_user = {
		.id = get_uniq_id(&users), .joined_channel = 0, .permissions = (unsigned int)cmd->perm};
	hset_insert_copy(&users, &new_user);
	return new_user.id;
}
static inline size_t commd_unregister_process(struct commd_unregister *cmd, enum commd_error *e) {
	/* DEBUGF("delete user %zu (admin %zu)\n", cmd->uid, cmd->auid); */
	if (cmd->auid != cmd->uid && !user_has_permission(cmd->auid, perm_unregister_user)) return_error(ERR_ACCESS);
	varusr.id = cmd->uid;
	hset_remove(&users, &varusr);
	return cmd->uid;
}
static inline size_t commd_create_process(struct commd_create *cmd, enum commd_error *e) {
	if (!user_has_permission(cmd->uid, perm_add_channel)) return_error(ERR_ACCESS);

	pthread_t chid;
	int sock = -1;
	unsigned short port = 0;
	{
		pthread_mutex_t sock_mx = PTHREAD_MUTEX_INITIALIZER;
		pthread_cond_t sock_cond = PTHREAD_COND_INITIALIZER;
		struct thread_loop_arg arg = {
			.owner = cmd->uid, .sock_dest = &sock, .sock_mx = &sock_mx, .sock_ready_cond = &sock_cond};
		pthread_mutex_lock(&sock_mx);
		chid = spawn_channel(&arg);
		DEBUGF("\n"); 
		while (sock == -1 || port == 0) pthread_cond_wait(&sock_cond, &sock_mx);
		DEBUGF("\n");
		pthread_mutex_unlock(&sock_mx);
	}
	struct tcp_channel new_channel = {.owner = cmd->uid, .name = NULL, .fd = sock};
	hset_insert_copy(&channels, &new_channel);
	return chid;
}
static inline size_t commd_delete_process(struct commd_delete *cmd, enum commd_error *e) {
	DEBUGF("received command%p\n", (void *)cmd);
	varchnl.id = cmd->chid;
	struct tcp_channel *c = hset_at(&channels, &varchnl);
	if (c == NULL) return_error(ERR_PARAM);
	if (cmd->uid != c->owner && !user_has_permission(cmd->uid, perm_unadd_channel)) return_error(ERR_ACCESS);
	hset_remove(&channels, &varchnl);
	return varchnl.id;
}
static inline size_t commd_join_process(struct commd_join *cmd, enum commd_error *e) {
	if (cmd->uid != cmd->juid && !user_has_permission(cmd->uid, perm_join_user)) return_error(ERR_ACCESS);
	struct kv_system_packet packet = {
		.magic_bytes = SYS_PACKET_MAGIC_BYTES, .operation_id = htonl(SYS_JOIN), .user_id = cmd->juid};
	if (!sendto_channel(cmd->chid, &packet, TCP_MAX_WAIT_MS, TCP_MAX_RETRIES)) return_error(ERR_SERV);
	return (size_t)get_channel_port(cmd->chid);
}
static inline size_t commd_leave_process(struct commd_leave *cmd, enum commd_error *e) {
	if (cmd->uid != cmd->luid && !user_has_permission(cmd->uid, perm_kick_user)) return_error(ERR_ACCESS);
	struct kv_system_packet packet = {
		.magic_bytes = SYS_PACKET_MAGIC_BYTES, .operation_id = htonl(SYS_LEAVE), .user_id = cmd->luid};
	if (!sendto_channel(cmd->chid, &packet, TCP_MAX_WAIT_MS, TCP_MAX_RETRIES)) return_error(ERR_SERV);
	return 1;
}
/// switches on command type and operates accordingly
static size_t process_cmd(enum commd_type type, struct commd *cmd, enum commd_error *NONNULL e) {
	// network byte order conversion
	switch (type) {
	case CMD_LEAVE:
	case CMD_JOIN: ((struct commd_conv *)cmd)->_3 = ntohzu(((struct commd_conv *)cmd)->_3); FALLTHROUGH;
	case CMD_UNREGISTER:
	case CMD_REGISTER:
	case CMD_DELETE: ((struct commd_conv *)cmd)->_2 = ntohzu(((struct commd_conv *)cmd)->_2); FALLTHROUGH;
	case CMD_CREATE:
	case CMD_GET_PORT: ((struct commd_conv *)cmd)->_1 = ntohzu(((struct commd_conv *)cmd)->_1); FALLTHROUGH;
	case CMD_GET_CHANNELS:;
	}
	// processing
	switch (type) {
	case CMD_REGISTER: return commd_register_process((struct commd_register *)cmd, e);
	case CMD_UNREGISTER: return commd_unregister_process((struct commd_unregister *)cmd, e);
	case CMD_CREATE: return commd_create_process((struct commd_create *)cmd, e);
	case CMD_DELETE: return commd_delete_process((struct commd_delete *)cmd, e);
	case CMD_JOIN: return commd_join_process((struct commd_join *)cmd, e);
	case CMD_LEAVE: return commd_leave_process((struct commd_leave *)cmd, e);
	case CMD_GET_PORT: return (size_t)get_channel_port(((struct commd_get_port *)cmd)->cihd);
	case CMD_GET_CHANNELS: return_error(ERR_DO_IT_YOURSELF);
	}
	return_error(ERR_PARAM);
}
static void process_connection(int sockfd) {
	DEBUGF("PROCESSING CONNECTION\n");
	// TODO: protection against blocking reads
	enum commd_type type;
	if (read(sockfd, &type, sizeof(type)) != sizeof(type)) report_error(sockfd, ERR_INVAL);
	type = ntohl(type);
	struct commd cmd;
	memset(&cmd, 0, sizeof(cmd)); // TODO: consider to remove
	ssize_t commd_size = commd_size_lookup[type];
	if (read(sockfd, &cmd, commd_size) != commd_size) report_error(sockfd, ERR_INVAL);
	enum commd_error e = ERR_SUCCESS;
	size_t cmd_status = process_cmd(type, &cmd, &e);
	if (e == ERR_DO_IT_YOURSELF) cmd_status = send_channels(sockfd, &e);
	cmd_status = htonzu(cmd_status);
	if (e != ERR_SUCCESS) report_error(sockfd, e);
	write(sockfd, &cmd_status, sizeof(cmd_status));
}

pthread_t spawn_channel(struct thread_loop_arg *arg) {
	pthread_t thread;
	pthread_create(&thread, NULL, thread_loop, arg);
	return thread;
}
bool sendto_channel(size_t chid, struct kv_system_packet *packet, int wait_ack_ms, int repeat) {
	bool success = wait_ack_ms == 0;
	varchnl.id = chid;
	struct tcp_channel *ch = hset_at(&channels, &varchnl);
	if (ch == NULL) return false;
	set_sock_timeout(udpctlfd, wait_ack_ms);

	struct sockaddr_in chaddr = {0};
	socklen_t len = sizeof(chaddr);
	getsockname(ch->fd, (struct sockaddr *)&chaddr, &len);
	do {
		sendto(udpctlfd, packet, KV_PACKET_SIZE, 0, (struct sockaddr *)&chaddr, len);
		if (wait_ack_ms == 0) continue;
		struct kv_system_packet resp;
		recvfrom(udpctlfd, &resp, KV_PACKET_SIZE, 0, (struct sockaddr *)&chaddr, &len);
		if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
		if (resp.operation_id == SYS_ACK) success = true;
	} while (--repeat >= 0);

	return success;
}
void tcp_loop(void) {
	init_statics();
	init_admin(ADMIN_UID);
	int sock = setup_socket(TCP_PORT);
	if (listen(sock, LISTEN_AMOUNT) != 0) {
		perror("listen on TCP socket failed");
		exit(EXIT_FAILURE);
	}
	DEBUGF("listening on port %hu\n", TCP_PORT);
	struct sockaddr_in accept_addr;
	socklen_t addrlen = sizeof(accept_addr);
	int currfd;
	while (!should_exit) {
		currfd = accept(sock, (struct sockaddr *)&accept_addr, &addrlen);
		if (currfd < 0) continue;
		DEBUGF("accepted connection on port %hu\n", accept_addr.sin_port);
		process_connection(currfd);
		shutdown(currfd, SHUT_RDWR);
		close(currfd);
	}
	close(sock);
	close(udpctlfd);
	hset_free(&users);
	hset_free(&channels);
}
