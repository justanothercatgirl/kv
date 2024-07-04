#ifndef KV_SERVER_TCP
#define KV_SERVER_TCP

#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>
#include <packet.h>
#include "channel.h"

#include <container.h>


#define TCP_PORT      8085
#define LISTEN_AMOUNT 128


// val: struct tcp_channel
extern struct hash_set channels;
// val: struct tcp_user
extern struct hash_set users;

void print_state(int);
void exit_tcp(int);

void tcp_loop(void);
pthread_t spawn_channel(struct thread_loop_arg *arg);
bool sendto_channel(size_t chid, struct kv_system_packet* packet, int wait_ack_ms, int repeat);

#endif // KV_SERVER_TCP
