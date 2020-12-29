#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "libclocksync.h"

/* comment this out ONLY if you are SURE the caller won't use this in multithreaded fashion.
 * be aware that if it does, it will eventually crash unless this is defined! */
#define MULTITHREAD_AWARE
/* uncomment for debugging purposes--the above mentioned condition is far more obvious if we force ourself to constantly open/close the segment */
#define ALWAYS_CLOSE_SHMEM_AGAIN
/* really stress-test the code by rapidly opening and closing the shared memory segment.
 * if there's a window of opportunity for thread 1 to access the segment while thread 2 is closing/reopening
 * this ensures it's much more likely to happen----SEGFAULT! */
#define BE_AN_ASSHOLE

#if defined(MULTITHREAD_AWARE)
#include <pthread.h>
#endif

static int				shmem_fd = -1;
static volatile clocksync_shmem*	shmem_ptr = NULL;
static double				shmem_diff = 0;

#if defined(MULTITHREAD_AWARE)
/* we must have a mutex, in case the caller is multithreaded.
 * this way, we don't fuck up if called from multiple threads at once */
pthread_mutex_t				shmem_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* caller must hold mutex */
int clocksync_open() {
	/* our actions will modify caller's errno. we don't want that.
	 * threading safety: GLIBC makes errno thread local storage, so no concurrency issues arise doing this */
	const int o_errno = errno;

	if (shmem_fd >= 0 || shmem_ptr != NULL)
		return 0;

	if ((shmem_fd = open(CLOCKSYNC_SHMEM_PATH,O_RDONLY)) < 0)
		{ errno = o_errno; return 1; }

	shmem_ptr = (volatile clocksync_shmem*)mmap(NULL,4096,PROT_READ,MAP_SHARED,shmem_fd,0);
	if (shmem_ptr == (volatile clocksync_shmem*)(-1)) {
		errno = o_errno;
		close(shmem_fd);
		return 1;
	}

	/* restore errno */
	errno = o_errno;
	return 0;
}

/* caller must hold mutex */
int clocksync_close() {
	/* our actions will modify caller's errno. we don't want that.
	 * threading safety: GLIBC makes errno thread local storage, so no concurrency issues arise doing this */
	const int o_errno = errno;

	if (shmem_ptr) {
		munmap((void*)shmem_ptr,4096);
		shmem_ptr = NULL;
	}
	if (shmem_fd >= 0) {
		close(shmem_fd);
		shmem_fd = -1;
	}

	/* restore errno */
	errno = o_errno;
	return 0;
}

/* can be called anywhere, thread-safe */
double clocksync_local() {
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return ((double)tv.tv_sec) + (((double)tv.tv_usec) / 1000000);
}

/* this takes the mutex */
double clocksync_master_local_delta() {
#if defined(MULTITHREAD_AWARE)
	/* we're intended to be a function that gets the time and QUICKLY returns.
	 * if one thread is held up for whatever reason with open() taking forever
	 * then at least have the courtesy to not hold up other threads trying to
	 * make this call too */
	if (pthread_mutex_trylock(&shmem_mutex) == 0) {
#endif
		if (!shmem_ptr)
			clocksync_open();

		if (shmem_ptr) {
			if (shmem_ptr->sig == CLOCKSYNC_SIG) {
/*				time_t now = time(NULL); */

				/* check for cases where the file we have open is stale and the daemon
				 * is writing to another file */
				struct stat s;
				if (fstat(shmem_fd,&s))
					clocksync_close();
				else if (!S_ISREG(s.st_mode))
					clocksync_close();
				/* if it was deleted, note that too */
				else if (s.st_nlink < 1)
					clocksync_close();
				else
					shmem_diff = shmem_ptr->time;
			}
			else {
				clocksync_close();
			}

#if defined(ALWAYS_CLOSE_SHMEM_AGAIN)
			/* stress-test closing and opening, intentionally cause a situation that
			 * invalidates the shmem region, to catch other concurrent threads using
			 * the pointer without taking the mutex */
			clocksync_close();
#  if defined(BE_AN_ASSHOLE)
			{
				/* serious stress-testing: opening and closing rapidly greatly increases
				 * the chance of hitting the window if the mutex isn't preventing other
				 * threads from doing this concurrently. if concurrency is the problem
				 * we will most likely crash ourself and show it. */
				unsigned int asshole_counter = 100;
				do {
					clocksync_open();
					clocksync_close();
				} while (--asshole_counter != 0);
			}
#  endif
#endif
		}

		double ret = shmem_diff;
#if defined(MULTITHREAD_AWARE)
		pthread_mutex_unlock(&shmem_mutex);
#endif
		return ret;
#if defined(MULTITHREAD_AWARE)
	}

	/* this is probably not a 100% sane way to do this, but on Pentium and higher systems
	 * with FPU we can assume the compiler assigns floating point using FLD, and is therefore
	 * atomic, and will not produce half-updated floating point numbers this way */
	return shmem_diff;
#endif
}

double clocksync_master() {
	return clocksync_local()+clocksync_master_local_delta();
}

