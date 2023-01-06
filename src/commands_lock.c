/* CSP Posix implementation of libparam command locks */

#include <semaphore.h>

#include <param/param_scheduler.h>
#include <time.h>

static sem_t commands_lock = {0};

int param_commands_lock_take(int block_time_ms) {

		int ret;

	if (block_time_ms == -1) {
		ret = sem_wait(&commands_lock);
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

		ret = sem_timedwait(&commands_lock, &ts);
	}

	if (ret != 0)
		return -1;

	return 0;
}

void param_commands_lock_give(void) {
	int value;
	sem_getvalue(&commands_lock, &value);
	if (value > 0) {
		return;
	}

	if (sem_post(&commands_lock) == 0) {
		return;
	}

	return;
}

void param_commands_lock_init(void) {
	sem_init(&commands_lock, 0, 1);
}


