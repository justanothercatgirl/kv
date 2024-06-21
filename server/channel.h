#ifndef KV_SERVER_CHANNEL_H
#define KV_SERVER_CHANNEL_H

#include <stdbool.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>

#ifdef DEBUG
#define DEBUGF(fmt, ...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define DEBUGF(fmt, ...) \
    do { } while (0)
#endif

#define KV_PACKET_SIZE 512

enum system_operation {
	do_nothing = 0,
	join_channel = -1,
	shutdown_socket = -2,
	acknowledgement = -3,
};
struct user {
	unsigned int ip;
	unsigned short port;
	int id;
};
struct channel_handle {
	int sockfd;
	struct user* users;
};
struct kv_packet {
	int id;
	unsigned char data[KV_PACKET_SIZE - sizeof(unsigned int)];
};
struct kv_system_packet {
	int operation_id;
	int user_id;
	int return_address;
	unsigned short return_port;
	unsigned int checksum;
	unsigned char sentinel[KV_PACKET_SIZE - 4 * sizeof(int) - sizeof(short)];
};

void thread_loop(void);

struct channel_handle *channel_init(void);
void channel_uninit(struct channel_handle *handle);

enum system_operation handle_system_packet(struct kv_packet* packet, struct channel_handle* handle);
unsigned int system_packet_checksum(struct kv_system_packet *packet);
void send_packets_back(struct kv_packet** packets, struct channel_handle *handle);

void send_cancellation_messages(struct channel_handle *handle) ;
void clear_packet_array(struct kv_packet **array);

int __user_cmp(const void* a, const void* b);

#endif // KV_SERVER_CHANNEL_H
