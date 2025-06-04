#pragma once

#include <slash/slash.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int py_init_interpreter(void);
extern int py_apm_load_cmd(struct slash *slash);

#ifdef __cplusplus
}
#endif
