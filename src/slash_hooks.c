
#include "loki.h"
#include "python/python_loader.h"


void slash_on_execute_hook(const char *line) {
	on_loki_slash_execute_hook(line);
    on_python_slash_execute_pre_hook(line);
}

void slash_on_execute_post_hook(const char *line, struct slash_command *command) {
    on_python_slash_execute_post_hook(line, command);
}
