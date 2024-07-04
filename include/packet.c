#include "packet.h"

ssize_t const commd_size_lookup[64] = {
	[CMD_CREATE] = sizeof(struct commd_create),
	[CMD_DELETE] = sizeof(struct commd_delete),
	[CMD_JOIN] = sizeof(struct commd_join),
	[CMD_LEAVE] = sizeof(struct commd_leave),
	[CMD_REGISTER] = sizeof(struct commd_register),
	[CMD_UNREGISTER] = sizeof(struct commd_unregister),
	[CMD_GET_PORT] = sizeof(struct commd_get_port),
	[CMD_GET_CHANNELS] = 0, // You can not get a sizeof(void)
	0};

unsigned int system_packet_checksum(struct kv_system_packet *packet) {
	return ((packet->user_id << 8) ^ (ntohzu(packet->operation_id) | (177013 << 10)) ^ packet->ackid) &
	       packet->magic_bytes;
}
char is_system_packet(struct kv_packet *p) {
	struct kv_system_packet *sysp = (struct kv_system_packet *)p;
	if (sysp->magic_bytes == SYS_PACKET_MAGIC_BYTES && (signed int)ntohl(sysp->operation_id) < 0) return 1;
	return 0;
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
size_t htonzu(size_t a) {
	#if __SIZEOF_SIZE_T__ == 8
	return ((uint64_t)htonl((uint32_t)(a & 0xFFFFFFFF)) << 32) | (uint64_t)htonl((uint32_t)(a >> 32));
	#else
	return htonl((uint32_t)a);
	#endif
}
size_t ntohzu(size_t a) {
	#if __SIZEOF_SIZE_T__ == 8
	return ((uint64_t)ntohl((uint32_t)(a & 0xFFFFFFFF)) << 32) | (uint64_t)ntohl((uint32_t)(a >> 32));
	#else
	return ntohl((uint32_t)a);
	#endif
}
#else
size_t htonzu(size_t a) { return a; }
size_t ntohzu(size_t a) { return a; }
#endif

const char *kv_strerror(enum commd_error e) {
	switch (e) {
	case ERR_SUCCESS: return "Success";
	case ERR_SERV: return "Internal server error";
	case ERR_ACCESS: return "No access";
	case ERR_INVAL: return "Invalid/insufficient parameters";
	case ERR_PARAM: return "Incorrect parameters";
	case ERR_NOIMPL: return "Not implemented";
	case ERR_DO_IT_YOURSELF: return "How do you have this error????";
	}
	return "Unknown error";
}
