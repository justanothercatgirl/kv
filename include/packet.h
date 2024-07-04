#ifndef KV_PACKET_H
#define KV_PACKET_H

#define KV_PACKET_SIZE 512

#ifdef DEBUG
	#define DEBUGF(fmt, ...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
	#define DEBUGF(fmt, ...)
#endif
#ifdef __clang__
	#pragma GCC diagnostic ignored "-Wnullability-extension" // SHUT UP I PUT IT BEHIND A MACRO
	#define NONNULL _Nonnull
	#define NULLABLE _Nullable
#else
	#define NONNULL
	#define NULLABLE
#endif

#include <arpa/inet.h>
#include <stddef.h>

// all ones so that ntohl is not needed
#define SYS_PACKET_MAGIC_BYTES 0xFFFFFFFFU

extern ssize_t const commd_size_lookup[];

enum system_operation {
	SYS_KEEPALIVE = (int)0x80000000, // -2^31
	SYS_JOIN,		 	 // 1 - 2^31
	SYS_LEAVE,		 	 // 2 - 2^31
	SYS_ACK,	     		 // 3 - 2^31
	SYS_KYS,			 // 4 - 2^31
}; 

struct kv_packet {
	size_t uid;
	unsigned char data[KV_PACKET_SIZE - sizeof(size_t)];
};
/// ONLY `operation_id` field is in network byte order. all other fields are 
struct kv_system_packet {
	const unsigned int magic_bytes /* = 0xFFFFFFFF */;
	// as in system_operation enum.
	int operation_id;
	// could be ignored
	size_t user_id;
	// id to be returned in acknowledgement. if 0, don't acknowledge.
	unsigned int ackid;
	// calculated with system_packet_checksum function
	unsigned int checksum;
	// TODO: add server signature here

	unsigned char sentinel[KV_PACKET_SIZE - 4 * sizeof(int) - sizeof(size_t)];
};

unsigned int system_packet_checksum(struct kv_system_packet *packet);
char is_system_packet(struct kv_packet *p);

enum permissions {
	perm_none	 	= 0,
	perm_register_user	= 1 << 1,
	perm_unregister_user	= 1 << 2,
	perm_add_channel 	= 1 << 3,
	perm_unadd_channel 	= 1 << 4,
	perm_join_user	 	= 1 << 5,
	perm_kick_user	 	= 1 << 6,
	perm_admin		= 0x7FFFFFFF,
};

enum commd_error {
	// No error
	ERR_SUCCESS,
	// Internal server error
	ERR_SERV,
	// Error in parameters (e.g. nonexistant UID)
	ERR_PARAM,
	// Invalid parameters (e.g. not enough params)
	ERR_INVAL,
	// Access violation
	ERR_ACCESS,
	// Operation not implemented yet
	ERR_NOIMPL,
	// Internal indication that function can not process request
	ERR_DO_IT_YOURSELF, // should never be sent out to suer
};

struct tcp_user {
	size_t id;
	size_t joined_channel;
	int permissions;
	char *pubkey;
};
struct tcp_channel {
	pthread_t id;
	size_t owner;
	int fd;
	char *name;
};

enum commd_type {
	CMD_CREATE = 0,
	CMD_DELETE,
	CMD_JOIN,
	CMD_LEAVE,
	CMD_REGISTER,
	CMD_UNREGISTER,
	CMD_GET_PORT,
	CMD_GET_CHANNELS,
};
// Somehow, this is pretty similar to linux sockaddr.
struct commd {
	unsigned char command[32];
};
struct commd_register {
	// Administrator UID. Optional.
	size_t auid;
	// Permissions
	size_t perm;
};
struct commd_unregister {
	// Administrator UID. Optional.
	size_t auid;
	// UID to be unregistered.
	size_t uid;
};
struct commd_create {
	// UID of user creating the channel.
	size_t uid;
};
struct commd_delete {
	// UID of user deleting a channel.
	size_t uid;
	// CHID of channel being deleted.
	pthread_t chid;
};
struct commd_join {
	// UID of user performing operation.
	size_t uid;
	// UID of user to be joined to channel.
	size_t juid;
	// CHID of the channel to be joined.
	pthread_t chid;
};
struct commd_leave {
	// UID of user performing operation.
	size_t uid;
	// UID of user leaving channel.
	size_t luid;
	// CHID of channel to be left by user `luid`.
	pthread_t chid;
};
struct commd_get_port {
	// CHID of channel to get the UDP port of
	size_t cihd;
};
typedef void commd_get_channels;
struct commd_conv {
	size_t _1;
	size_t _2; 
	size_t _3;
	size_t _4;
};

size_t htonzu(size_t a);
size_t ntohzu(size_t a);

const char* kv_strerror(enum commd_error e);

#endif // KV_PACKET_H
