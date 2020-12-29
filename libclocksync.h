#ifndef __LIBCLOCKSYNC_H
#define __LIBCLOCKSYNC_H

#include <stdint.h>

/* clocksync library: read master-slave delta from clocksync daemon */

#define CLOCKSYNC_SHMEM_PATH	"/dev/shm/clocksync"

#define CLOCKSYNC_SIG		0x12345678

#define CLOCKSYNC_PORT		23000

typedef struct {
	uint32_t		sig;
	float			time;		/* time delta */
} clocksync_shmem;

#ifdef __cplusplus
extern "C" {
#endif

int clocksync_open();
int clocksync_close();
double clocksync_local();
double clocksync_master();
double clocksync_master_local_delta();

#ifdef __cplusplus
}
#endif

#endif
