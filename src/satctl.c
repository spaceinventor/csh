/*
 * Copyright (c) 2014 Satlab ApS <satlab@satlab.com>
 * Proprietary software - All rights reserved.
 *
 * Satellite and subsystem control program
 */

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
#include <csp/arch/csp_thread.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/interfaces/csp_if_udp.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_collector.h>

#include "csp_if_tun.h"
#include "prometheus.h"
#include "param_sniffer.h"
#include "crypto.h"
#include "tfetch.h"
#include "csp_if_eth.h"

#define SATCTL_PROMPT_GOOD		    "\033[96msatctl \033[90m%\033[0m "
#define SATCTL_PROMPT_BAD		    "\033[96msatctl \033[31m!\033[0m "
#define SATCTL_DEFAULT_CAN_DEV	    "can0"
#define SATCTL_DEFAULT_UART_DEV	    "/dev/ttyUSB0"
#define SATCTL_DEFAULT_UART_BAUD    1000000
#define SATCTL_DEFAULT_ADDRESS		1
#define SATCTL_LINE_SIZE		    128
#define SATCTL_HISTORY_SIZE		    2048

VMEM_DEFINE_STATIC_RAM(test, "test", 100000);
VMEM_DEFINE_FILE(tfetch, "tfetc", "tfetch.vmem", 120);
VMEM_DEFINE_FILE(col, "col", "colcnf.vmem", 120);
VMEM_DEFINE_FILE(csp, "csp", "cspcnf.vmem", 120);
VMEM_DEFINE_FILE(params, "param", "params.csv", 50000);
VMEM_DEFINE_FILE(crypto, "crypto", "crypto.csv", 50000);

void usage(void)
{
	printf("usage: satctl [command]\n");
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

int main(int argc, char **argv)
{
	static struct slash *slash;
	int remain, index, i, c, p = 0;
	char *ex;

	uint16_t addr = SATCTL_DEFAULT_ADDRESS;
	char *can_dev = SATCTL_DEFAULT_CAN_DEV;
	char *uart_dev = SATCTL_DEFAULT_UART_DEV;
	uint32_t uart_baud = SATCTL_DEFAULT_UART_BAUD;
	int use_uart = 0;
	int use_can = 0;
	int use_prometheus = 0;
	char * udp_peer_str[10];
	int udp_peer_idx = 0;
	int csp_version = 2;
	char * rtable = NULL;
	char * tun_conf_str = NULL;
	char * eth_ifname = NULL;
	char * csp_zmqhub_addr[10];
	int csp_zmqhub_idx = 0;

	while ((c = getopt(argc, argv, "+hpr:b:c:u:n:v:R:t:e:z:")) != -1) {
		switch (c) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'r':
			udp_peer_str[udp_peer_idx++] = optarg;
			break;
		case 'c':
			use_can = 1;
			can_dev = optarg;
			break;
		case 'u':
			use_uart = 1;
			uart_dev = optarg;
			break;
		case 'b':
			uart_baud = atoi(optarg);
			break;
		case 'n':
			addr = atoi(optarg);
			break;
		case 'p':
			use_prometheus = 1;
			break;
		case 'R':
			rtable = optarg;
			break;
		case 'v':
			csp_version = atoi(optarg);
			break;
		case 't':
			tun_conf_str = optarg;
			break;
		case 'e':
			eth_ifname = optarg;
			break;
		case 'z':
			csp_zmqhub_addr[csp_zmqhub_idx++] = optarg;
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}

	remain = argc - optind;
	index = optind;

	if (use_can == 0 && use_uart == 0 && udp_peer_idx == 0 && csp_zmqhub_addr == NULL) {
		printf("\n");
		printf(" *** Warning: No interfaces configured ***\n");
		printf("  use -c for CAN\n");
		printf("  use -u for UART\n");
		printf("  use -r for UDP\n");
		printf("  use -z for ZMQHUB\n");
	}

	/* Get csp config from file */
	vmem_file_init(&vmem_csp);

	csp_conf.address = addr;
	csp_conf.version = csp_version;
	csp_conf.hostname = "satctl";
	csp_conf.model = "linux";
	csp_init();

	//csp_debug_set_level(4, 1);
	//csp_debug_set_level(5, 1);

	csp_iface_t * default_iface = NULL;
	if (use_uart) {
		csp_usart_conf_t conf = {
			.device = uart_dev,
			.baudrate = uart_baud, /* supported on all platforms */
			.databits = 8,
			.stopbits = 1,
			.paritysetting = 0,
			.checkparity = 0
		};
		int error = csp_usart_open_and_add_kiss_interface(&conf, CSP_IF_KISS_DEFAULT_NAME, &default_iface);
		if (error != CSP_ERR_NONE) {
			csp_log_error("failed to add KISS interface [%s], error: %d", uart_dev, error);
			exit(1);
		}
	}

	if (use_can) {
		int error = csp_can_socketcan_open_and_add_interface(can_dev, CSP_IF_CAN_DEFAULT_NAME, 1000000, true, &default_iface);
		if (error != CSP_ERR_NONE) {
			csp_log_error("failed to add CAN interface [%s], error: %d", can_dev, error);
		}
	}

	if (csp_route_start_task(0, 0) < 0)
		return -1;

	csp_rdp_set_opt(3, 10000, 5000, 1, 2000, 2);
	//csp_rdp_set_opt(10, 20000, 8000, 1, 5000, 9);

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

	if (tun_conf_str) {

		int src;
		int dst;

		if (sscanf(tun_conf_str, "%d %d", &src, &dst) != 2) {
			printf("Invalid TUN configuration string: %s\n", tun_conf_str);
			printf("Should math the pattern \"<src> <dst>\" exactly\n");
			return -1;
		}

		csp_iface_t * tun_if = malloc(sizeof(csp_iface_t));
		csp_if_tun_conf_t * ifconf = malloc(sizeof(csp_if_tun_conf_t));

		ifconf->tun_dst = dst;
		ifconf->tun_src = src;

		csp_if_tun_init(tun_if, ifconf);

	}

	if (eth_ifname) {
		static csp_iface_t csp_iface_eth;
		csp_if_eth_init(&csp_iface_eth, eth_ifname);
		default_iface = &csp_iface_eth;
	}

	while (csp_zmqhub_idx > 0) {
		char * zmq_str = csp_zmqhub_addr[--csp_zmqhub_idx];
		printf("zmq str %s\n", zmq_str);
		csp_iface_t * zmq_if;
		csp_zmqhub_init(csp_get_address(), zmq_str, 0, &zmq_if);

		/* Use auto incrementing names */
		char * zmq_name = malloc(20);
		sprintf(zmq_name, "ZMQ%u", csp_zmqhub_idx);
		zmq_if->name = zmq_name;

		default_iface = zmq_if;
	}

	if (!rtable) {
		/* Read routing table from parameter system */
		extern param_t csp_rtable;
		char saved_rtable[csp_rtable.array_size];
		param_get_string(&csp_rtable, saved_rtable, csp_rtable.array_size);
		rtable = saved_rtable;
	}

	if (csp_rtable_check(rtable)) {
		int error = csp_rtable_load(rtable);
		if (error < 1) {
			csp_log_error("csp_rtable_load(%s) failed, error: %d", rtable, error);
			exit(1);
		}
	} else if (default_iface) {
		printf("Setting default route to %s\n", default_iface->name);
		csp_rtable_set(0, 0, default_iface, CSP_NO_VIA_ADDRESS);
	} else {
		printf("No routing defined\n");
	}


	csp_socket_t *sock_csh = csp_socket(CSP_SO_NONE);
	csp_socket_set_callback(sock_csh, csp_service_handler);
	csp_bind(sock_csh, CSP_ANY);

	csp_socket_t *sock_param = csp_socket(CSP_SO_NONE);
	csp_socket_set_callback(sock_param, param_serve);
	csp_bind(sock_param, PARAM_PORT_SERVER);

	csp_thread_handle_t vmem_handle;
	csp_thread_create(vmem_server_task, "vmem", 2000, NULL, 1, &vmem_handle);

	slash = slash_create(SATCTL_LINE_SIZE, SATCTL_HISTORY_SIZE);
	if (!slash) {
		fprintf(stderr, "Failed to init slash\n");
		exit(EXIT_FAILURE);
	}

	/* Parameters */
	vmem_file_init(&vmem_params);
	param_list_store_vmem_load(&vmem_params);

	/* Start a collector task */
	vmem_file_init(&vmem_col);
	pthread_t param_collector_handle;
	pthread_create(&param_collector_handle, NULL, &param_collector_task, NULL);

	if (use_prometheus) {
		prometheus_init();
		param_sniffer_init();
	}

	/* Crypto magic */
	vmem_file_init(&vmem_crypto);
	crypto_key_refresh();

	/* Test of time fetch */
	vmem_file_init(&vmem_tfetch);
	tfetch_onehz();

	/* Interactive or one-shot mode */
	if (remain > 0) {
		ex = malloc(SATCTL_LINE_SIZE);
		if (!ex) {
			fprintf(stderr, "Failed to allocate command memory");
			exit(EXIT_FAILURE);
		}

		/* Build command string */
		for (i = 0; i < remain; i++) {
			if (i > 0)
				p = ex - strncat(ex, " ", SATCTL_LINE_SIZE - p);
			p = ex - strncat(ex, argv[index + i], SATCTL_LINE_SIZE - p);
		}
		slash_execute(slash, ex);
		free(ex);
	} else {
		printf("\n\n");
		printf(" *******************************\n");
		printf(" **   Satctl - Space Command  **\n");
		printf(" *******************************\n\n");

		printf(" Copyright (c) 2021 Space Inventor ApS <info@space-inventor.com>\n");
		printf(" Copyright (c) 2014 Satlab ApS <satlab@satlab.com>\n\n");

		slash_loop(slash, SATCTL_PROMPT_GOOD, SATCTL_PROMPT_BAD);
	}

	slash_destroy(slash);

	return 0;
}
