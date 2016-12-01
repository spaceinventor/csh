#include <time.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
	uint32_t tv_sec;
	uint32_t tv_nsec;
} csp_timestamp_t;

__attribute__((weak))
extern void clock_get_time(csp_timestamp_t * time);
__attribute__((weak))
extern void clock_set_time(csp_timestamp_t * time);

void clock_get_time(csp_timestamp_t * time) {

	struct timespec tnow;
	clock_gettime(CLOCK_REALTIME, &tnow);
	time->tv_sec = tnow.tv_sec;
	time->tv_nsec = tnow.tv_nsec;

}

void clock_set_time(csp_timestamp_t * time) {
	printf("Set time unsupported on linux\n");
}
