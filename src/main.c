#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/utsname.h>
#include <time.h>

#include <slash/slash.h>
#include <slash/dflopt.h>

#include <csp/csp.h>
#include <csp/csp_yaml.h>
#include <csp/csp_hooks.h>

#include <curl/curl.h>
#include "loki.h"

#include <param/param.h>
#ifdef PARAM_HAVE_COMMANDS
#include <param/param_commands.h>
#endif
#ifdef PARAM_HAVE_SCHEDULER
#include <param/param_scheduler.h>
#endif

#include <vmem/vmem_file.h>

#include "known_hosts.h"

extern const char *version_string;
extern int loki_running;

#define PROMPT_BAD		    "\x1b[0;38;5;231;48;5;31;1m csh \x1b[0;38;5;31;48;5;236;22m! \x1b[0m "
#define LINE_SIZE		    512
#define HISTORY_SIZE		2048

VMEM_DEFINE_FILE(col, "col", "colcnf.vmem", 120);
#ifdef PARAM_HAVE_COMMANDS
VMEM_DEFINE_FILE(commands, "cmd", "commands.vmem", 2048);
#endif
#ifdef PARAM_HAVE_SCHEDULER
VMEM_DEFINE_FILE(schedule, "sch", "schedule.vmem", 2048);
#endif

int slash_prompt(struct slash * slash) {

	int len = 0;
	int fore = 255;
	int back = 33;

	fflush(stdout);
	printf("\e[0;38;5;%u;48;5;%u;1m ", fore, back);
	len += 1;

	struct utsname info;
	uname(&info);
	printf("%s", info.nodename);
	len += strlen(info.nodename);
		
	if (slash_dfl_node != 0) {

		fore = back;
		back = 240;
		printf(" \e[0;38;5;%u;48;5;%u;22m ", fore, back);
		fore = 255;
		printf("\e[0;38;5;%u;48;5;%u;1m", fore, back);
		len += 3;

		char nodebuf[20];
		if (known_hosts_get_name(slash_dfl_node, nodebuf, sizeof(nodebuf)) == 0) {
			/* Failed to find hostname, only print node */
			snprintf(nodebuf, 20, "%d", slash_dfl_node);
		} else {
			/* Found hostname, now append node */
			char nodenumbuf[7];
			snprintf(nodenumbuf, 7, "@%d", slash_dfl_node);
			strncat(nodebuf, nodenumbuf, 20-strnlen(nodebuf, 20));
		}
		printf("%s", nodebuf);
		len += strlen(nodebuf);

	}

	extern param_queue_t param_queue;
	if (param_queue.type == PARAM_QUEUE_TYPE_GET) {

		fore = back;
		back = 34;
		printf(" \e[0;38;5;%u;48;5;%u;22m ", fore, back);
		fore = 255;
		printf("\e[0;38;5;%u;48;5;%u;1m", fore, back);
		len += 3;

		printf("\u2193 %s", param_queue.name);
		len += 2 + strlen(param_queue.name);

	}

	extern param_queue_t param_queue;
	if (param_queue.type == PARAM_QUEUE_TYPE_SET) {

		fore = back;
		back = 124;
		printf(" \e[0;38;5;%u;48;5;%u;22m ", fore, back);
		fore = 255;
		printf("\e[0;38;5;%u;48;5;%u;1m", fore, back);
		len += 3;

		printf("\u2191 %s", param_queue.name);
		len += 2 + strlen(param_queue.name);

	}

	/* End of breadcrumb */
	fore = back;
	printf(" \e[0m\e[0;38;5;%um \e[0m", fore);
	len += 3;




	fflush(stdout);

	return len;

}

uint64_t clock_get_nsec(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1E9 + ts.tv_nsec;
}

void usage(void) {
	printf("usage: csh -i init.csh [command]\n");
	printf("\n");
	printf("Copyright (c) 2016-2023 Space Inventor A/S <info@space-inventor.com>\n");
	printf("\n");
}

#ifdef PARAM_HAVE_SCHEDULER
void * onehz_task(void * param) {
	while(1) {
		csp_timestamp_t scheduler_time = {};
		csp_clock_get_time(&scheduler_time);
		param_schedule_server_update(scheduler_time.tv_sec * 1E9 + scheduler_time.tv_nsec);
		sleep(1);
	}
	return NULL;
}

static int cmd_sch_update(struct slash *slash) {

	static pthread_t onehz_handle;
	pthread_create(&onehz_handle, NULL, &onehz_task, NULL);

	return SLASH_SUCCESS;
}
slash_command_sub(param_server, start, cmd_sch_update, "", "Update param server each second");
#endif

	
int main(int argc, char **argv) {

	static struct slash *slash;
	int remain, index, i, c, p = 0;

	char * initfile = "init.csh";
	char * dirname = getenv("HOME");

	while ((c = getopt(argc, argv, ":+hi:")) != -1) {
		switch (c) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'i':
			dirname = "";
			initfile = optarg;
			break;
		default:
			printf("Argument -%c not recognized\n", c);
			exit(EXIT_FAILURE);
		}
	}

	remain = argc - optind;
	index = optind;

	if (remain == 0) {
		printf("\033[33m\n");
		printf("  ***********************\n");
		printf("  **     CSP   Shell   **\n");
		printf("  ***********************\n\n");

		printf("\033[32m");
		printf("  Copyright (c) 2016-2023 Space Inventor A/S <info@space-inventor.com>\n");
		printf("  Compiled: %s git: %s\n\n", __DATE__, version_string);

		printf("\033[0m");
	} else {
		printf("\033[33m\n");
		printf("  CSP shell batch: ");
		printf("\033[0m");
		printf("\n");
		
	}
	srand(time(NULL));

 /** curl_global_init() should be invoked exactly once for each application that
 * uses libcurl and before any call of other libcurl functions.
 * This function is not thread-safe! */
    curl_global_init(CURL_GLOBAL_ALL);

	void serial_init(void);
	serial_init();

	slash = slash_create(LINE_SIZE, HISTORY_SIZE);
	if (!slash) {
		fprintf(stderr, "Failed to init slash\n");
		exit(EXIT_FAILURE);
	}

#ifdef PARAM_HAVE_COMMANDS
	vmem_file_init(&vmem_commands);
	param_command_server_init();
#endif
#ifdef PARAM_HAVE_SCHEDULER
	param_schedule_server_init();
#endif

	/** Persist hosts file */
	char * homedir = getenv("HOME");
	char path[100];

	if (strlen(homedir)) {
		snprintf(path, 100, "%s/csh_hosts", homedir);
	} else {
		snprintf(path, 100, "csh_hosts");
	}

	slash_run(slash, path, 0);




	/* Init file */
	char buildpath[100];
	if (strlen(dirname)) {
		snprintf(buildpath, 100, "%s/%s", dirname, initfile);
	} else {
		snprintf(buildpath, 100, "%s", initfile);
	}
	printf("\033[34m  Init file: %s\033[0m\n", buildpath);
	slash_run(slash, buildpath, 0);

	int ret = 0;
	/* Interactive or one-shot mode */
	if (remain > 0) {
		char ex[LINE_SIZE] = {};

		/* Build command string */
		for (i = 0; i < remain; i++) {
			if (i > 0)
				p = ex - strncat(ex, " ", LINE_SIZE - p);
			p = ex - strncat(ex, argv[index + i], LINE_SIZE - p);
		}

		usleep(500000);
		printf("\n");
		strcpy(slash->buffer, ex);
		slash->length = strlen(slash->buffer);
		slash_refresh(slash, 1);
		printf("\n");

        if(loki_running){
            int ex_len = strlen(ex);
            char * dup = malloc(ex_len + 2);
            strncpy(dup, ex, ex_len);
            dup[ex_len] = '\n';
            dup[ex_len + 1] = '\0';
            loki_add(dup, 1);
            free(dup);
        }

		ret = slash_execute(slash, ex);
	} else {
		printf("\n\n");

		slash_loop(slash);
	}

	printf("\n");
	slash_destroy(slash);
    curl_global_cleanup();

	return ret;
}
