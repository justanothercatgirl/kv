#include "packet.h"

unsigned int system_packet_checksum(struct kv_system_packet *packet) {
	return 	(packet->user_id << 8) ^ 
		(packet->operation_id | (177013 << 10));
}

