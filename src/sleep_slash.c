#include <stdlib.h>
#include <slash/slash.h>

static int cmd_sleep(slash_t *slash) {

	if (slash->argc != 2) {
        return SLASH_EUSAGE;
	}

    slash_wait_interruptible(slash, atoi(slash->argv[1]));
	return SLASH_SUCCESS;
}
slash_command(sleep, cmd_sleep, "<sleep ms>", "Sleep the specified amount of milliseconds");
