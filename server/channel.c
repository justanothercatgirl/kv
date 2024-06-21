#include "channel.h"

#include <container.h>
#include <stdlib.h>
#include <errno.h>

void thread_loop(void) {
	struct channel_handle *channel = channel_init();
	struct kv_packet **recvd_data = array_new(struct kv_packet*, 100);
	struct kv_packet work_buffer;
	size_t recvd_index = 0;
	while (true) {
		struct sockaddr_in client_addr;
		socklen_t client_addr_len;
		int recvlength = recvfrom(channel->sockfd, &work_buffer, 
					  KV_PACKET_SIZE, MSG_DONTWAIT, 
					  (struct sockaddr*)&client_addr, &client_addr_len);
		if (recvlength > 0) {
			DEBUGF("rec_id = %i\n", work_buffer.id);
			if (work_buffer.id <= 0) handle_system_packet(&work_buffer, channel);
			struct kv_packet *kv_copy = malloc(KV_PACKET_SIZE);
			memcpy(kv_copy, &work_buffer, KV_PACKET_SIZE);
			++recvd_index;
			if (recvd_index >= array_size(recvd_data)) {
				array_push(recvd_data, kv_copy);
			} else {
				recvd_data[recvd_index] = kv_copy;
			}
		} else if (errno == EWOULDBLOCK) {
			if (array_size(recvd_data) == 0) {
				// TODO: don't set O_NONBLOCK and wait for data
				continue;
			}
			send_packets_back(recvd_data, channel);
			clear_packet_array(recvd_data);
		} else {
			perror("thread_loop failed");
		}
	}
	channel_uninit(channel);
}

struct channel_handle *channel_init(void) {
	struct sockaddr_in thread_local_address = {
		.sin_family = AF_INET, 
		.sin_port = 0, 
		.sin_addr = {INADDR_ANY}
	};
	struct channel_handle *handle = NULL;
	char chain_result = /* This is evil, but */
		(handle = (struct channel_handle*)calloc(1, sizeof(struct channel_handle))) != NULL
		&& (handle->users = array_new(struct user, 0)) != NULL 
		&& (handle->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) >= 0 
		&& bind(handle->sockfd, (struct sockaddr*)&thread_local_address, sizeof(thread_local_address)) == 0;
	if (!chain_result) {
		perror("channel init failed");
		if (handle) {
			if (handle->users) array_free(handle->users);
			if (handle->sockfd >= 0) close(handle->sockfd);
			free(handle);
		}
		return NULL;
	}
	DEBUGF("Channel [%i] created\n", handle->sockfd);
	return handle;
}

void channel_uninit(struct channel_handle *handle) {
	array_free(handle->users);
	if (close(handle->sockfd) == -1) perror("could not gracefully uninitialize channel");
	free(handle);
}

enum system_operation handle_system_packet(struct kv_packet* packet, struct channel_handle* handle) {
	struct kv_system_packet* spacket = (struct kv_system_packet*) packet;
	if (system_packet_checksum(spacket) != spacket->checksum) return do_nothing;
	switch (spacket->operation_id) {
	case do_nothing: return do_nothing;
	case join_channel: {
		DEBUGF("someone joined the channel: id=%i\n", spacket->user_id);
		struct user user = {
			.ip = ntohl(spacket->return_address),
			.port = ntohs(spacket->return_port),
			.id = ntohl(spacket->user_id)
		};
		if (!array_binary_search(handle->users, &user, __user_cmp)) {
			array_push(handle->users, user);
			array_qsort(handle->users, __user_cmp);
		}
		struct sockaddr_in reply_addr = {.sin_family = AF_INET, .sin_port = spacket->return_port, .sin_addr = {spacket->return_address} };
		struct kv_system_packet reply = {.user_id = 0, .return_address = 0, .return_port = 0, .operation_id = acknowledgement};
		sendto(handle->sockfd, &reply, KV_PACKET_SIZE,  0, (struct sockaddr*)&reply_addr, sizeof(reply_addr));
	} return do_nothing;
	case shutdown_socket: return shutdown_socket;
	default: return do_nothing;
	}
}

void send_packets_back(struct kv_packet** packets, struct channel_handle* handle) {
	/*DEBUGF("");*/
	for (size_t i = 0; i < array_size(handle->users); ++i) {
		struct user* current_user = &handle->users[i];
		struct sockaddr_in destination = {
			.sin_family = AF_INET, 
			.sin_port = htons(current_user->port), 
			.sin_addr = { htonl(current_user->ip) }
		};
		for (size_t j = 0; packets[j] != NULL && j < array_size(packets); ++j) {
			DEBUGF("sending packet with id %i", packets[j]->id);
			DEBUGF("to destination: %u.%u.%u.%u:%hu\n", 
					(destination.sin_addr.s_addr >> 24) & 0xFF, 
					(destination.sin_addr.s_addr >> 16) & 0xFF, 
					(destination.sin_addr.s_addr >> 8) & 0xFF, 
					destination.sin_addr.s_addr & 0xFF,
					destination.sin_port);
			if (packets[j]->id == current_user->id) continue;
			int error_code = sendto(handle->sockfd, packets[j], KV_PACKET_SIZE, 0, (struct sockaddr*)&destination, sizeof(destination));
			if (error_code) perror("could not send packets back"); 
		}
	}
}

void clear_packet_array(struct kv_packet **array) {
	for (size_t i = 0 ; i < array_size(array); ++i) {
		if (array[i] == NULL) return;
		free(array[i]);
		array[i] = NULL;
	}
}

