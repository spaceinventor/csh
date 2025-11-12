
#include "loki.h"
#ifdef HAVE_PYTHON
#include "python/python_loader.h"
#endif

void slash_on_execute_hook(const char *line) {
	on_loki_slash_execute_hook(line);
#ifdef HAVE_PYTHON
    on_python_slash_execute_pre_hook(line);
#endif
}

void slash_on_execute_post_hook(const char *line, struct slash_command *command) {
#ifdef HAVE_PYTHON
    on_python_slash_execute_post_hook(line, command);
#endif
}
