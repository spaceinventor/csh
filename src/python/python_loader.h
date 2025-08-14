#pragma once

#include <slash/slash.h>

#ifdef __cplusplus
extern "C" {
#endif

void on_python_slash_execute_pre_hook(const char *line);
void on_python_slash_execute_post_hook(const char *line, struct slash_command *command);

extern void py_init_interpreter(void);
extern int py_apm_load_cmd(struct slash *slash);

extern bool exception_allowed;

#ifdef __cplusplus
}
#endif
