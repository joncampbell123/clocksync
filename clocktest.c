#define _XOPEN_SOURCE 500

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "libclocksync.h"

int main() {
	while (1) {
		double local = clocksync_local();
		double delta = clocksync_master_local_delta();
		double master = local + delta;

		printf("local: %.3f   master: %.3f   delta: %.3f\n",local,master,delta);
		usleep(1000);
	}

	return 0;
}

