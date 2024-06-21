#include "packet.h"

unsigned int system_packet_checksum(struct kv_system_packet *packet) {
	return 	(packet->user_id << 8) ^ 
		(packet->operation_id | (177013 << 10));
}

int __user_cmp(const void* a, const void* b) {
	struct user *_a = (struct user*)a,
		    *_b = (struct user*)b;
	return _a->id - _b->id;
}
