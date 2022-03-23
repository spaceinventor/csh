#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/utsname.h>

#include <slash/slash.h>

#include <csp/csp.h>
#include <csp/csp_yaml.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_collector.h>

#include <vmem/vmem_server.h>
#include <vmem/vmem_file.h>

#include "prometheus.h"
#include "param_sniffer.h"

extern const char *version_string;

#define PROMPT_GOOD		    "\033[96mcmd %\033[0m "
#define PROMPT_BAD		    "\033[91mcmd !\033[0m "
#define LINE_SIZE		    128
#define HISTORY_SIZE		2048

VMEM_DEFINE_FILE(col, "col", "colcnf.vmem", 120);
VMEM_DEFINE_FILE(params, "param", "params.csv", 50000);
VMEM_DEFINE_FILE(dummy, "dummy", "dummy.txt", 1000000);


void usage(void) {
	printf("usage: csh -f conf.yaml [command]\n");
	printf("\n");
	printf("Copyright (c) 2016-2022 Space Inventor ApS <info@space-inventor.com>\n");
	printf("\n");
	printf("Options:\n");
	printf(" -f\t\tPath to config file\n");
	printf(" -p\t\tSetup prometheus node\n");
	printf(" -R RTABLE\tOverride rtable with this string\n");
	printf(" -h\t\tPrint this help and exit\n");
}

void kiss_discard(char c, void * taskwoken) {
	putchar(c);
}

void * param_collector_task(void * param) {
	param_collector_loop(param);
	return NULL;
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

	/* Parameters */
	vmem_file_init(&vmem_params);
	param_list_store_vmem_load(&vmem_params);

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

	/* Start a collector task */
	vmem_file_init(&vmem_col);

	static pthread_t param_collector_handle;
	pthread_create(&param_collector_handle, NULL, &param_collector_task, NULL);

	static pthread_t router_handle;
	pthread_create(&router_handle, NULL, &router_task, NULL);

	static pthread_t vmem_server_handle;
	pthread_create(&vmem_server_handle, NULL, &vmem_server_task, NULL);

	if (use_prometheus) {
		prometheus_init();
		param_sniffer_init();
	}

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

		slash_loop(slash, PROMPT_GOOD, PROMPT_BAD);
	}

	printf("\n");
	slash_destroy(slash);

	return 0;
}
