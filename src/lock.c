/* CSP Posix implementation of libparam command locks */

#include <semaphore.h>
#include <stdint.h>
#include <time.h>

int si_lock_take(void* lock, int block_time_ms) {

	int ret;

	if (block_time_ms == -1) {
		ret = sem_wait(lock);
	} else {
		struct timespec ts;
		if (clock_gettime(CLOCK_REALTIME, &ts)) {
			return -1;
		}

		uint32_t sec = block_time_ms / 1000;
		uint32_t nsec = (block_time_ms - 1000 * sec) * 1000000;

		ts.tv_sec += sec;

		if (ts.tv_nsec + nsec >= 1000000000) {
			ts.tv_sec++;
		}

		ts.tv_nsec = (ts.tv_nsec + nsec) % 1000000000;

		ret = sem_timedwait(lock, &ts);
	}

	if (ret != 0)
		return -1;

	return 0;
}

int si_lock_give(void* lock) {

	int value;
	sem_getvalue(lock, &value);
	if (value > 0) {
		return 0;
	}

	if (sem_post(lock) == 0) {
		return 0;
	}

	return 0;
}

#define NUM_LOCKS 3

static uint8_t lock_taken[NUM_LOCKS] = {0};
static sem_t locks[NUM_LOCKS] = {0};

void* si_lock_init() {

	for (int i = 0; i < NUM_LOCKS; i++) {
		if(lock_taken[i] == 0) {
			lock_taken[i] = 1;
			sem_init(&locks[i], 0, 1);
			return &locks[i];
		}
	}

	return NULL;
}


