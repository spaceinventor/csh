#define  _GNU_SOURCE
#ifdef HAVE_PYTHON
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/utsname.h>
#include <time.h>
#include <signal.h>

#include "python/python_loader.h"
#include <slash/slash.h>
#include <apm/csh_api.h>
#include <csp/csp.h>
#include <csp/csp_hooks.h>

#include <curl/curl.h>

#include <param/param.h>
#ifdef PARAM_HAVE_COMMANDS_CLIENT
#include <param/param_commands.h>
#endif
#ifdef PARAM_HAVE_SCHEDULER
#include <param/param_scheduler.h>
#endif

#include <vmem/vmem_file.h>
#include <vmem/vmem_client.h>

#include <apm/environment.h>
#include "slash_env_var_completion.h"

extern const char *version_string;

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

#define CMD_NUM_ELEMENTS 0x200
#define SCH_NUM_ELEMENTS 0x200

VMEM_DEFINE_FILE(cmd_hash, "cmd_hash", "cmd_hash.vmem", 4*CMD_NUM_ELEMENTS);
VMEM_DEFINE_FILE(cmd_store, "cmd_store", "cmd_store.vmem", 0x200*CMD_NUM_ELEMENTS);
VMEM_DEFINE_FILE(sch_hash, "sch_hash", "sch_hash.vmem", 4*SCH_NUM_ELEMENTS);
VMEM_DEFINE_FILE(sch_store, "sch_store", "sch_store.vmem", 0x100*SCH_NUM_ELEMENTS);

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

		char nodebuf[CSP_HOSTNAME_LEN] = {0};
		if (known_hosts_get_name(slash_dfl_node, nodebuf, sizeof(nodebuf)-1) == 0) {
			/* Failed to find hostname, only print node */
			snprintf(nodebuf, CSP_HOSTNAME_LEN, "%d", slash_dfl_node);
		} else {
			/* Found hostname, now append node */
			char nodenumbuf[7] = {0};  // Longest string is "@16383\0"
			snprintf(nodenumbuf, 7, "@%d", slash_dfl_node);

			int node_idx = strnlen(nodebuf, CSP_HOSTNAME_LEN);
			const int overflow = (node_idx + strnlen(nodenumbuf, 7)) - CSP_HOSTNAME_LEN;
			if (overflow > 0) {
				node_idx -= overflow+1;
			}
			if (node_idx < 0) {
				node_idx = 0;
			}
			nodebuf[node_idx] = '\0';
			strncat(nodebuf, nodenumbuf, CSP_HOSTNAME_LEN-strnlen(nodebuf, CSP_HOSTNAME_LEN-1)-1);
		}
		printf("%s", nodebuf);
		len += strlen(nodebuf);

	}

#ifdef PARAM_HAVE_COMMANDS_CLIENT
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
#endif
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
	printf("Type 'manual' to open CSH manual\n");
	printf("\n");
	printf("Copyright (c) 2016-2025 Space Inventor A/S <info@space-inventor.com>\n");
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

/* Calling this variable "slash" somehow conflicts with __attribute__((section("slash"))) from "#define __slash_command()",
	causing the linker error: symbol `slash' is already defined */
static struct slash *slash2;
#define slash slash2

static void csh_cleanup(void) {
	/* Clear the environment, freeing up memory */
	csh_clearenv();
	slash_destroy(slash);  // Restores terminal
	curl_global_cleanup();
}

sighandler_t old_handler = NULL;
static void sigint_handler(int signum) {
#ifdef HAVE_PYTHON
	if (exception_allowed && old_handler) {
		old_handler(signum);  // Very likely to the python signal handler for KeyboardInterrupt
	}

#endif
	slash_sigint(slash, signum);
	vmem_client_abort();
}

static char *csh_environ_slash_process_cmd_line_hook(const char *line) {
    char *expansion = csh_expand_vars(line);
    /* NULL check is not performed on purpose here, SLASH is able to deal with this */
    return expansion;
}


int main(int argc, char **argv) {

	int remain, index, i, c, p = 0;
	int ret = SLASH_SUCCESS;

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
		case ':':
			switch (optopt)
			{
			case 'i':
				dirname = "";
				initfile = "";
				break;
			default:
				fprintf(stderr, "Argument -%c is missing a required argument\n", optopt);
				exit(EXIT_FAILURE);
			}
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
		printf("  Copyright (c) 2016-2025 Space Inventor A/S <info@space-inventor.com>\n");
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

	/* 
	 * Configure "slash_process_cmd_line_hook" with 
	 * our function that expands environment variables
	 */
	slash_process_cmd_line_hook = csh_environ_slash_process_cmd_line_hook;
	slash_global_completer = env_var_ref_completer;
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

	if (homedir && strlen(homedir)) {
		snprintf(path, 100, "%s/csh_hosts", homedir);
	} else {
		snprintf(path, 100, "csh_hosts");
	}
	if (access(path, F_OK) == 0) {
		slash_run(slash, path, 0);
	}

	/* Init file */
	char buildpath[100];
	if (dirname && strlen(dirname)) {
		snprintf(buildpath, 100, "%s/%s", dirname, initfile);
	} else {
		snprintf(buildpath, 100, "%s", initfile);
	}

	{  /* Setting environment variables for init script to use. */

		#define INIT_FILE "INIT_FILE"
		#define INIT_DIR "INIT_DIR"

		/* Set environment variable for init script file/directory, before running it. */
		char pathbuf[PATH_MAX] = {0};
		if (NULL == realpath(buildpath, pathbuf)) {
			pathbuf[0] = '\0';
		}
		/* NOTE: pathbuf can be a directory here, which is not the intention.
			It's a pretty inconsequential edge case though, so there's not much need to fix it. */
		csh_putvar(INIT_FILE, pathbuf);

		/* Init file found, and space for trailing '/' */
		if (strnlen(pathbuf, PATH_MAX) > 0) {

			/* Find init script dir by terminating at the last '/'.
				Removing the last '/' will keep init scripts more readable. */
			/* NOTE: This will not handle multiple invalid init files involving '/', such as:
				-i init.csh/
				-i /home/user  # <-- User being a directory
			*/
			char *last_slash = strrchr(pathbuf, '/');
			if (last_slash < pathbuf+PATH_MAX-1) {
				*(last_slash) = '\0';
			}
		}

		csh_putvar(INIT_DIR, pathbuf);
	}

#ifdef HAVE_PYTHON
	py_init_interpreter();
#endif

	/* Explicit init file given, or default file exists */
	if ((strlen(dirname) == 0 && strlen(initfile) > 0) || access(buildpath, F_OK) == 0) {
		printf("\033[34m  Init file: %s\033[0m\n", buildpath);
		ret = slash_run(slash, buildpath, 0);
	}

	{	/* Setting up signal handlers */

		atexit(csh_cleanup);

		old_handler = signal(SIGINT, sigint_handler);
		if (old_handler == SIG_ERR) {
			perror("signal");
			exit(EXIT_FAILURE);
		}

		if (signal(SIGTERM, sigint_handler) == SIG_ERR) {
			perror("signal");
			exit(EXIT_FAILURE);
		}
	}

	/* Interactive or one-shot mode */
	if (ret != SLASH_EXIT && remain > 0) {
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

		ret = slash_execute(slash, ex);
	} else if (ret != SLASH_EXIT) {
		printf("\n\n");

		ret = slash_loop(slash);

		if(ret == -ENOTTY){
			printf("No TTY detected running as non-interactive\n");
			while(1){
				sleep(10);
			}
		}
	}

	/* Cleanup handled by atexit() */
	return ret;
}
