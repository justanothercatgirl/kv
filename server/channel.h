#ifndef KV_SERVER_CHANNEL_H
#define KV_SERVER_CHANNEL_H

#include <kv.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>

#include <stdbool.h>

// main function that manages every channel
void thread_loop(void);

struct channel_handle *channel_init(void);
void channel_uninit(struct channel_handle *handle);

void send_packets_back(struct kv_packet** packets, struct channel_handle *handle);
void handle_system_packet(struct kv_packet* packet, struct channel_handle* handle);

void clear_packet_array(struct kv_packet **array);


#endif // KV_SERVER_CHANNEL_H
