#include <bits/types/sigset_t.h>
#define CONTAINER_IMPLEMENTATION
#define HSET_MAX_BUCKET_SIZE 4
#include <container.h>
#undef CONTAINER_IMPLEMENTATION

#include "tcp.h"

#include <signal.h>

#define MAIN_PORT 8164

void setup_signal(void) {
	struct sigaction signal = {.sa_handler = print_state, .sa_mask = {{0}}, .sa_flags = 0};
	if (sigaction(SIGUSR1, &signal, NULL) != 0) {
		DEBUGF("");
		exit(EXIT_FAILURE);
	}
	signal.sa_handler = exit_tcp;
	if (sigaction(SIGTERM, &signal, NULL) != 0) {
		DEBUGF("");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;
	setup_signal();
	tcp_loop();
	return 0;
}
