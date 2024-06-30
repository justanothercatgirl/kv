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
	keepalive = (int) 0x80000000, 	// -2^31
	join_channel, 			// 1 - 2^31
	leave_channel, 			// 2 - 2^31
	acknowledgement,		// 3 - 2^31
};

struct kv_packet {
	int id;
	unsigned char data[KV_PACKET_SIZE - sizeof(unsigned int)];
};
struct kv_system_packet {
	// as in system_operation enum.
	int operation_id;
	// could be ignored
	int user_id;
	// calculated with system_packet_checksum function
	unsigned int checksum;
	
	unsigned char sentinel[KV_PACKET_SIZE - 3 * sizeof(int)];
};

unsigned int system_packet_checksum(struct kv_system_packet *packet);
int __user_cmp(const void* a, const void* b);

#endif // KV_PACKET_H

