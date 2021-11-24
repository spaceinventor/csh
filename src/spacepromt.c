#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <slash/slash.h>

#include <param/param.h>
#include <vmem/vmem_server.h>
#include <vmem/vmem_ram.h>
#include <vmem/vmem_file.h>

#include <csp/csp.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/interfaces/csp_if_udp.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>
#include <csp/csp_iflist.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_collector.h>

#include "csp_if_tun.h"
#include "prometheus.h"
#include "param_sniffer.h"
#include "crypto.h"
#include "csp_if_eth.h"


#define SPACEPROMT_PROMPT_GOOD		    "\033[34mspacepromt \033[90m%\033[0m "
#define SPACEPROMT_PROMPT_BAD		    "\033[34mspacepromt \033[31m!\033[0m "
#define SPACEPROMT_DEFAULT_CAN_DEV	    "can0"
#define SPACEPROMT_DEFAULT_UART_DEV	    "/dev/ttyUSB0"
#define SPACEPROMT_DEFAULT_UART_BAUD    1000000
#define SPACEPROMT_DEFAULT_ADDRESS		1
#define SPACEPROMT_LINE_SIZE		    128
#define SPACEPROMT_HISTORY_SIZE		    2048

VMEM_DEFINE_STATIC_RAM(test, "test", 100000);
VMEM_DEFINE_FILE(col, "col", "colcnf.vmem", 120);
VMEM_DEFINE_FILE(csp, "csp", "cspcnf.vmem", 120);
VMEM_DEFINE_FILE(params, "param", "params.csv", 50000);
VMEM_DEFINE_FILE(crypto, "crypto", "crypto.csv", 50000);

void usage(void)
{
	printf("usage: spacepromt [command]\n");
	printf("\n");
	printf("Copyright (c) 2018 Space Inventor ApS <info@spaceinventor.com>\n");
	printf("Copyright (c) 2014 Satlab ApS <satlab@satlab.com>\n");
	printf("\n");
	printf("Options:\n");
	printf(" -c INTERFACE,\tUse INTERFACE as CAN interface\n");
	printf(" -u INTERFACE,\tUse INTERFACE as UART interface\n");
	printf(" -b BAUD,\tUART buad rate\n");
	printf(" -n NODE\tUse NODE as own CSP address\n");
	printf(" -r UDP_CONFIG\tUDP configuration string, encapsulate in brackets: \"<lport> <peer ip> <rport>\" (supports multiple) \n");
	printf(" -z ZMQ_IP\tIP of zmqproxy node (supports multiple)\n");
	printf(" -p\t\tSetup prometheus node\n");
	printf(" -R RTABLE\tOverride rtable with this string\n");
	printf(" -h\t\tPrint this help and exit\n");
}

void kiss_discard(char c, void * taskwoken) {
	putchar(c);
}

static pthread_t router_handle;
void * router_task(void * param) {
	while(1) {
		csp_route_work();
	}
}
	
int main(int argc, char **argv)
{

	printf("\033[33m\n");
	printf("  *********************\n");
	printf("  **   Space Shell   **\n");
	printf("  *********************\n\n");

	printf("\033[32m");
	printf("  Copyright (c) 2016-2022 Space Inventor ApS <info@space-inventor.com>\n\n");

	printf("\033[0m");

	static struct slash *slash;
	int remain, index, i, c, p = 0;
	char *ex;

	int use_prometheus = 0;
	int csp_version = 2;
	char * rtable = NULL;
	char * yamlname = "iflist.yaml";
	
	while ((c = getopt(argc, argv, "+hpv:R:f:")) != -1) {
		switch (c) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'p':
			use_prometheus = 1;
			break;
		case 'R':
			rtable = optarg;
			break;
		case 'v':
			csp_version = atoi(optarg);
			break;
		case 'f':
			yamlname = optarg;
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}

	remain = argc - optind;
	index = optind;

	/* Parameters */
	vmem_file_init(&vmem_params);
	param_list_store_vmem_load(&vmem_params);

	csp_conf.version = csp_version;
	csp_conf.hostname = "spacepromt";
	csp_conf.model = "linux";
	csp_init();

	csp_debug_set_level(4, 1);
	//csp_debug_set_level(5, 1);

	iflist_yaml_init(yamlname);

	pthread_create(&router_handle, NULL, &router_task, NULL);

	csp_rdp_set_opt(3, 10000, 5000, 1, 2000, 2);
	//csp_rdp_set_opt(10, 20000, 8000, 1, 5000, 9);

#if 0

	while (udp_peer_idx > 0) {
		char * udp_str = udp_peer_str[--udp_peer_idx];
		printf("udp str %s\n", udp_str);

		int lport = 9600;
		int rport = 9600;
		char udp_peer_ip[20];

		if (sscanf(udp_str, "%d %19s %d", &lport, udp_peer_ip, &rport) != 3) {
			printf("Invalid UDP configuration string: %s\n", udp_str);
			printf("Should math the pattern \"<lport> <peer ip> <rport>\" exactly\n");
			return -1;
		}

		csp_iface_t * udp_client_if = malloc(sizeof(csp_iface_t));
		csp_if_udp_conf_t * udp_conf = malloc(sizeof(csp_if_udp_conf_t));
		udp_conf->host = udp_peer_ip;
		udp_conf->lport = lport;
		udp_conf->rport = rport;
		csp_if_udp_init(udp_client_if, udp_conf);

		/* Use auto incrementing names */
		char * udp_name = malloc(20);
		sprintf(udp_name, "UDP%u", udp_peer_idx);
		udp_client_if->name = udp_name;

		default_iface = udp_client_if;
	}


	if (eth_ifname) {
		static csp_iface_t csp_iface_eth;
		csp_if_eth_init(&csp_iface_eth, eth_ifname);
		default_iface = &csp_iface_eth;
	}


#endif

	extern param_t csp_rtable;
	char saved_rtable[csp_rtable.array_size];

	if (!rtable) {
		/* Read routing table from parameter system */
		param_get_string(&csp_rtable, saved_rtable, csp_rtable.array_size);
		rtable = saved_rtable;
	}

	if (csp_rtable_check(rtable)) {
		int error = csp_rtable_load(rtable);
		if (error < 1) {
			csp_log_error("csp_rtable_load(%s) failed, error: %d", rtable, error);
		}
	}

	csp_socket_t *sock_csh = csp_socket(CSP_SO_NONE);
	csp_socket_set_callback(sock_csh, csp_service_handler);
	csp_bind(sock_csh, CSP_ANY);

	csp_socket_t *sock_param = csp_socket(CSP_SO_NONE);
	csp_socket_set_callback(sock_param, param_serve);
	csp_bind(sock_param, PARAM_PORT_SERVER);

	void * vmem_server_task(void * param) {
		vmem_server_loop(param);
		return NULL;
	}
	pthread_t vmem_server_handle;
	pthread_create(&vmem_server_handle, NULL, &vmem_server_task, NULL);

	slash = slash_create(SPACEPROMT_LINE_SIZE, SPACEPROMT_HISTORY_SIZE);
	if (!slash) {
		fprintf(stderr, "Failed to init slash\n");
		exit(EXIT_FAILURE);
	}

	/* Start a collector task */
	vmem_file_init(&vmem_col);

	void * param_collector_task(void * param) {
		param_collector_loop(param);
		return NULL;
	}
	pthread_t param_collector_handle;
	pthread_create(&param_collector_handle, NULL, &param_collector_task, NULL);

	if (use_prometheus) {
		prometheus_init();
		param_sniffer_init();
	}

	/* Crypto magic */
	vmem_file_init(&vmem_crypto);
	crypto_key_refresh();

	/* Interactive or one-shot mode */
	if (remain > 0) {
		ex = malloc(SPACEPROMT_LINE_SIZE);
		if (!ex) {
			fprintf(stderr, "Failed to allocate command memory");
			exit(EXIT_FAILURE);
		}

		/* Build command string */
		for (i = 0; i < remain; i++) {
			if (i > 0)
				p = ex - strncat(ex, " ", SPACEPROMT_LINE_SIZE - p);
			p = ex - strncat(ex, argv[index + i], SPACEPROMT_LINE_SIZE - p);
		}
		slash_execute(slash, ex);
		free(ex);
	} else {
		printf("\n\n");

		slash_loop(slash, SPACEPROMT_PROMPT_GOOD, SPACEPROMT_PROMPT_BAD);
	}

	slash_destroy(slash);

	return 0;
}
