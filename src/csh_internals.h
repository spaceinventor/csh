#pragma once

#include <slash/optparse.h>

#ifdef __cplusplus
extern "C" {
#endif

void rdp_opt_add(optparse_t * parser);
void rdp_opt_set();
void rdp_opt_reset();

#ifdef __cplusplus
}
#endif
