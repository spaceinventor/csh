#include <stdlib.h>
#include <unistd.h>
#include <slash/slash.h>

static int cmd_sleep(struct slash *slash) {

	if (slash->argc != 2) {
        return SLASH_EUSAGE;
	}
	usleep(atoi(slash->argv[1]) * 1000);
	return SLASH_SUCCESS;
}
slash_command(sleep, cmd_sleep, "<sleep ms>", "Sleep the specified amount of milliseconds");
