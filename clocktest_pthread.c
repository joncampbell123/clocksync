#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <pthread.h>

#include "libclocksync.h"

static void *my_thread(void *x) {
	double stop_at = clocksync_local() + (60*5);	/* <- run for 5 min */
	while (clocksync_local() < stop_at) {
		double x = clocksync_master();
//		printf("%.3f\n",x);
	}
}

int main() {
	/* make sure libclocksync can handle being called safely from multiple threads.
	 * so we dont have another Highland Dance scenario 2009 :( */
	/* ref: dvminigrab calls clocksync_master() from both threads: the main thread, and capture thread.
	 *      inevitably, the shmem pointer is caught at one point unmapped, and dvminigrab crashes */
	int threads;
	for (threads=0;threads < 200;threads++) {
		pthread_t xx;
		if (pthread_create(&xx,NULL,my_thread,NULL) != 0)
			printf("Error starting thread %d/50\n",threads+1);
	}

	sleep(60*5 + 10);
	return 0;
}

