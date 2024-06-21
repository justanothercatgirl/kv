#ifndef KV_PACKET_H
#define KV_PACKET_H

#define KV_PACKET_SIZE 512

#ifdef DEBUG
#define DEBUGF(fmt, ...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define DEBUGF(fmt, ...) \
    do { } while (0)
#endif

enum system_operation {
	do_nothing = 0,
	join_channel = -1,
	leave_channel = -2,
	acknowledgement = -4,
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
	unsigned int checksum;
	unsigned char sentinel[KV_PACKET_SIZE - 4 * sizeof(int) - sizeof(short)];
};

unsigned int system_packet_checksum(struct kv_system_packet *packet);
int __user_cmp(const void* a, const void* b);

#endif // KV_PACKET_H

