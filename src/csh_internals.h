#pragma once

#include <slash/optparse.h>

#ifdef __cplusplus
extern "C" {
#endif

void rdp_opt_add(optparse_t * parser);
void rdp_opt_set(void);
void rdp_opt_reset(void);
void serial_init(void);
uint32_t serial_get(void);
void* si_lock_init(void);
int si_lock_take(void* lock, int block_time_ms);
int si_lock_give(void* lock);

#ifdef __cplusplus
}
#endif
