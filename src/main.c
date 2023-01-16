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

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_collector.h>
#include <param/param_queue.h>
#include <param/param_commands.h>
#include <param/param_scheduler.h>

#include <vmem/vmem_server.h>
#include <vmem/vmem_file.h>

#include "prometheus.h"
#include "param_sniffer.h"
#include "known_hosts.h"

extern const char *version_string;

#define PROMPT_BAD		    "\x1b[0;38;5;231;48;5;31;1m csh \x1b[0;38;5;31;48;5;236;22m! \x1b[0m "
#define LINE_SIZE		    128
#define HISTORY_SIZE		2048

VMEM_DEFINE_FILE(col, "col", "colcnf.vmem", 120);
VMEM_DEFINE_FILE(commands, "cmd", "commands.vmem", 2048);
VMEM_DEFINE_FILE(schedule, "sch", "schedule.vmem", 2048);
VMEM_DEFINE_FILE(dummy, "dummy", "dummy.txt", 1000000);

#define PARAMID_TELEM1					 302
#define PARAMID_TELEM2					 303

uint16_t _telem1 = 0;
uint16_t _telem2 = 0;
PARAM_DEFINE_STATIC_RAM(PARAMID_TELEM1,      telem1,        PARAM_TYPE_UINT16, -1, 0, PM_TELEM, NULL, "", &_telem1, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_TELEM2,      telem2,        PARAM_TYPE_UINT16, -1, 0, PM_TELEM, NULL, "", &_telem2, NULL);

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
	printf("usage: csh -f conf.yaml [command]\n");
	printf("\n");
	printf("Copyright (c) 2016-2022 Space Inventor ApS <info@space-inventor.com>\n");
	printf("\n");
	printf("Options:\n");
	printf(" -f\t\tPath to config file\n");
	printf(" -n NODE\tUse NODE as own CSP address on the default interface\n");
	printf(" -v VERSION\tUse VERSION as CSP version (1 or 2)\n");
	printf(" -p\t\tSetup prometheus node\n");
	printf(" -r RTABLE\tOverride rtable with this string\n");
	printf(" -h\t\tPrint this help and exit\n");
}

void kiss_discard(char c, void * taskwoken) {
	putchar(c);
}

void * router_task(void * param) {
	while(1) {
		csp_route_work();
	}
}

void * vmem_server_task(void * param) {
	vmem_server_loop(param);
	return NULL;
}


void * onehz_task(void * param) {
	while(1) {
		csp_timestamp_t scheduler_time = {};
        csp_clock_get_time(&scheduler_time);
        param_schedule_server_update(scheduler_time.tv_sec * 1E9 + scheduler_time.tv_nsec);
		sleep(1);
	}
	return NULL;
}

	
int main(int argc, char **argv) {

	static struct slash *slash;
	int remain, index, i, c, p = 0;

	int use_prometheus = 0;
	int csp_version = 2;
	char * rtable = NULL;
	char * yamlname = "csh.yaml";
	char * dirname = getenv("HOME");
	unsigned int dfl_addr = 0;
	
	while ((c = getopt(argc, argv, "+hpn:v:r:f:")) != -1) {
		switch (c) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'p':
			use_prometheus = 1;
			break;
		case 'r':
			rtable = optarg;
			break;
		case 'v':
			csp_version = atoi(optarg);
			break;
		case 'n':
			dfl_addr = atoi(optarg);
			break;
		case 'f':
			dirname = "";
			yamlname = optarg;
			break;
		default:
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
		printf("  Copyright (c) 2016-2022 Space Inventor ApS <info@space-inventor.com>\n");
		printf("  Compiled: %s git: %s\n\n", __DATE__, version_string);

		printf("\033[0m");
	} else {
		printf("\033[33m\n");
		printf("  CSP shell batch: ");
		printf("\033[0m");
		printf("\n");
		
	}
	srand(time(NULL));
	
	void serial_init(void);
	serial_init();

	static char hostname[100];
	gethostname(hostname, 100);

	static char domainname[100];
	int res = getdomainname(domainname, 100);
	(void) res;

	struct utsname info;
	uname(&info);

	csp_conf.hostname = info.nodename;
	csp_conf.model = info.version;
	csp_conf.revision = info.release;
	csp_conf.version = csp_version;
	csp_conf.dedup = CSP_DEDUP_OFF;
	csp_init();

	//csp_debug_set_level(4, 1);
	//csp_debug_set_level(5, 1);

	if (strlen(dirname)) {
		char buildpath[100];
		snprintf(buildpath, 100, "%s/%s", dirname, yamlname);
		csp_yaml_init(buildpath, &dfl_addr);
	} else {
		csp_yaml_init(yamlname, &dfl_addr);

	}

	csp_rdp_set_opt(3, 10000, 5000, 1, 2000, 2);
	//csp_rdp_set_opt(5, 10000, 5000, 1, 2000, 4);
	//csp_rdp_set_opt(10, 10000, 5000, 1, 2000, 8);
	//csp_rdp_set_opt(25, 10000, 5000, 1, 2000, 20);
	//csp_rdp_set_opt(40, 3000, 1000, 1, 250, 35);

#if (CSP_HAVE_STDIO)
	if (rtable && csp_rtable_check(rtable)) {
		int error = csp_rtable_load(rtable);
		if (error < 1) {
			printf("csp_rtable_load(%s) failed, error: %d\n", rtable, error);
		}
	}
#endif
	(void) rtable;

	csp_bind_callback(csp_service_handler, CSP_ANY);
	csp_bind_callback(param_serve, PARAM_PORT_SERVER);

	slash = slash_create(LINE_SIZE, HISTORY_SIZE);
	if (!slash) {
		fprintf(stderr, "Failed to init slash\n");
		exit(EXIT_FAILURE);
	}

	vmem_file_init(&vmem_dummy);

	static pthread_t router_handle;
	pthread_create(&router_handle, NULL, &router_task, NULL);

	static pthread_t vmem_server_handle;
	pthread_create(&vmem_server_handle, NULL, &vmem_server_task, NULL);

	static pthread_t onehz_handle;
	pthread_create(&onehz_handle, NULL, &onehz_task, NULL);

	if (use_prometheus) {
		prometheus_init();
		param_sniffer_init();
	}

	/** Persist hosts file */
	char * homedir = getenv("HOME");
    char path[100];

	if (strlen(homedir)) {
        snprintf(path, 100, "%s/csh_hosts", homedir);
    } else {
        snprintf(path, 100, "csh_hosts");
    }

	vmem_file_init(&vmem_commands);
	param_command_server_init();
	param_schedule_server_init();

	slash_run(slash, path, 0);

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
		slash_execute(slash, ex);
	} else {
		printf("\n\n");

		slash_loop(slash);
	}

	printf("\n");
	slash_destroy(slash);

	return 0;
}
