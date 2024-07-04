#ifndef KV_SERVER_CHANNEL_H
#define KV_SERVER_CHANNEL_H

#include <kv.h>

#include <netinet/ip.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>

/// Required for the calling thread to set socket file descriptor
struct thread_loop_arg {
	int *sock_dest;
	pthread_mutex_t *sock_mx;
	pthread_cond_t *sock_ready_cond;
	size_t owner;
	const unsigned char *pubkey;
};

// main function that manages every channel
void *thread_loop(void *);

#endif // KV_SERVER_CHANNEL_H
