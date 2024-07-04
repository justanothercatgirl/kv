#include "channel.h"

#include <packet.h>
#include <container.h>
#include <errno.h>
#include <stdlib.h>

#define decisive_push(array, index, elem)                                                                              \
	do {                                                                                                           \
		if (index >= array_size(array)) {                                                                      \
			array_push(array, elem);                                                                       \
		} else {                                                                                               \
			array[index] = elem;                                                                           \
		}                                                                                                      \
	} while (0)

#if defined(__unix__) && defined(__GNUC__)
	#define __THREAD __thread
#elif __STDC_VERSION__ >= 201112L && __STDC_VERSION__ < 202302L
	#define __THREAD _Thread_local
#elif __STDC_VERSION__ >= 202302L
	#define __THREAD thread_local
#else
	#pragma GCC error "Use Linux under GCC, or C11 or later standards"
	#define __THREAD "don't compile"
#endif

struct ch_user {
	size_t id;
	unsigned int ip;
	unsigned short port;
	unsigned long last_keepalive;
};

static __THREAD struct hash_set users;
static __THREAD int sockfd;

static int __user_hset_cmp(const void *a, const void *b) {
	struct ch_user *_a = (struct ch_user *)a, *_b = (struct ch_user *)b;
	return _a->id - _b->id;
}
static size_t __user_hset_hsh(const void *a) { return ((struct ch_user *)a)->id; }
static void clear_packet_array(struct kv_packet **array) {
	for (size_t i = 0; i < array_size(array); ++i) {
		if (array[i] == NULL) return;
		free(array[i]);
		array[i] = NULL;
	}
}
bool channel_init(void) {
	struct sockaddr_in thread_local_address = {.sin_family = AF_INET, .sin_port = 0, .sin_addr = {INADDR_ANY}};
	char chain_result = /* This is evil, but who cares */
		hset_ok(users = hset_new(sizeof(struct ch_user), __user_hset_cmp, __user_hset_hsh)) &&
		(sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) >= 0 &&
		bind(sockfd, (struct sockaddr *)&thread_local_address, sizeof(thread_local_address)) == 0;
	if (!chain_result) {
		perror("channel init failed");
		if (hset_ok(users)) hset_free(&users);
		if (sockfd >= 0) close(sockfd);
		return false;
	}
	DEBUGF("Channel [%i] created\n", sockfd);
	return true;
}
static void channel_uninit(void) {
	hset_free(&users);
	if (close(sockfd) == -1) perror("could not gracefully uninitialize channel");
}
_Noreturn static inline void forced_cleanup(struct kv_packet **packets) {
	clear_packet_array(packets);
	array_free(packets);
	channel_uninit();
	pthread_exit(NULL);
}
static inline void report_socket(const struct thread_loop_arg *arg, int fd) {
	pthread_mutex_lock(arg->sock_mx);
	*(arg->sock_dest) = fd;
	pthread_cond_signal(arg->sock_ready_cond);
	pthread_mutex_unlock(arg->sock_mx);
}

// Returns whether thread should kill itself
static bool handle_system_packet(struct kv_packet *packet, struct sockaddr_in *src) {
	TODO; // account for `ackid`
	struct kv_system_packet *spacket = (struct kv_system_packet *)packet;
	if (system_packet_checksum(spacket) != spacket->checksum) return false;
	switch (ntohl(spacket->operation_id)) {
	case SYS_KEEPALIVE: {
		struct ch_user u = {.id = ntohzu(spacket->user_id), 0};
		struct ch_user *data = hset_at(&users, &u);
		data->last_keepalive = (int)time(NULL);
	} break;
	case SYS_JOIN: {
		struct ch_user u = {
			.id = /*ntohzu */ spacket->user_id,
			.ip = /*ntohl*/ src->sin_addr.s_addr,
			.port = /*htons*/ src->sin_port,
			.last_keepalive = (int)time(NULL)};
		hset_insert_copy(&users, &u);
	} break;
	case SYS_LEAVE: {
		struct ch_user u = {.id = ntohzu(spacket->user_id), 0};
		hset_remove(&users, &u);
	} break;
	case SYS_ACK: return false;
	case SYS_KYS:
		// TODO: verify that request was sent by main thread
		return true;
	}
	return false;
}
static void send_packets_back(struct kv_packet **packets) {
	hset_iter iter;
	for (hseti_begin(&users, &iter); !hseti_end(&iter); hseti_next(&iter)) {
		struct ch_user *current_user = hseti_get(&iter);
		struct sockaddr_in destination = {
			.sin_family = AF_INET,
			.sin_port = htons(current_user->port),
			.sin_addr = {htonl(current_user->ip)}};
		for (size_t j = 0; packets[j] != NULL && j < array_size(packets); ++j) {
			DEBUGF("sending packet with id %zu to destination: %u.%u.%u.%u:%hu\n", packets[j]->uid,
			       (destination.sin_addr.s_addr >> 24) & 0xFF, (destination.sin_addr.s_addr >> 16) & 0xFF,
			       (destination.sin_addr.s_addr >> 8) & 0xFF, destination.sin_addr.s_addr & 0xFF,
			       destination.sin_port);
			if (packets[j]->uid == current_user->id) continue;
			int error_code = sendto(
				sockfd, packets[j], KV_PACKET_SIZE, 0, (struct sockaddr *)&destination,
				sizeof(destination));
			if (error_code) perror("could not send packets back");
		}
	}
}

/// An example of how you should NOT write code
void *thread_loop(void *arg) {
	if (!channel_init()) pthread_exit(NULL);
	report_socket((struct thread_loop_arg *)arg, sockfd);
	struct kv_packet **recvd_data = array_new(struct kv_packet *, 100);
	struct kv_packet work_buffer;
	size_t recvd_index = 0;
	int recv_flag = MSG_DONTWAIT;
	while (true) {
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		ssize_t recvlength = recvfrom(
			sockfd, &work_buffer, KV_PACKET_SIZE, recv_flag, (struct sockaddr *)&client_addr,
			&client_addr_len);
		if (recvlength > 0) {
			if (is_system_packet(&work_buffer)) {
				bool kys = handle_system_packet(&work_buffer, &client_addr);
				if (kys) forced_cleanup(recvd_data);
				continue;
			}
			recv_flag |= MSG_DONTWAIT;
			struct kv_packet *kv_copy = malloc(KV_PACKET_SIZE);
			memcpy(kv_copy, &work_buffer, KV_PACKET_SIZE);
			++recvd_index;
			decisive_push(recvd_data, recvd_index, kv_copy);
		} else if (errno == EWOULDBLOCK) {
			if (array_size(recvd_data) == 0) {
				recv_flag &= ~MSG_DONTWAIT;
				continue;
			}
			send_packets_back(recvd_data);
			clear_packet_array(recvd_data);
		} else {
			perror("error in thread_loop");
		}
	}
}
