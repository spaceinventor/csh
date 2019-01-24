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
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>
#include <param/param_list.h>
#include <param/param_group.h>
#include <param/param_server.h>
#include <param/param_collector.h>

#include "prometheus.h"
#include "param_sniffer.h"

#define SATCTL_PROMPT_GOOD		    "\033[96msatctl \033[90m%\033[0m "
#define SATCTL_PROMPT_BAD		    "\033[96msatctl \033[31m!\033[0m "
#define SATCTL_DEFAULT_CAN_DEV	    "can0"
#define SATCTL_DEFAULT_UART_DEV	    "/dev/ttyUSB0"
#define SATCTL_DEFAULT_UART_BAUD    1000000
#define SATCTL_DEFAULT_ADDRESS		0
#define SATCTL_LINE_SIZE		    128
#define SATCTL_HISTORY_SIZE		    2048

VMEM_DEFINE_STATIC_RAM(test, "test", 100000);
VMEM_DEFINE_FILE(col, "col", "colcnf.vmem", 120);
VMEM_DEFINE_FILE(csp, "csp", "cspcnf.vmem", 120);
VMEM_DEFINE_FILE(params, "param", "params.csv", 50000);

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
	printf(" -r REMOTE_IP\tSetup UDP peer\n");
	printf(" -p\t\tSetup prometheus node\n");
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

	uint8_t addr = SATCTL_DEFAULT_ADDRESS;
	char *can_dev = SATCTL_DEFAULT_CAN_DEV;
	char *uart_dev = SATCTL_DEFAULT_UART_DEV;
	uint32_t uart_baud = SATCTL_DEFAULT_UART_BAUD;
	int use_uart = 0;
	int use_can = 1;
	int use_prometheus = 0;
	char * udp_peer_ip = "";

	while ((c = getopt(argc, argv, "+hpr:b:c:u:n:")) != -1) {
		switch (c) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'r':
			udp_peer_ip = optarg;
			break;
		case 'c':
			use_uart = 0;
			use_can = 1;
			can_dev = optarg;
			break;
		case 'u':
			use_uart = 1;
			use_can = 0;
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
		default:
			exit(EXIT_FAILURE);
		}
	}

	remain = argc - optind;
	index = optind;

	/* Get csp config from file */
	vmem_file_init(&vmem_csp);

	if (csp_buffer_init(100, 320) < 0)
		return -1;

	csp_conf_t csp_config;
	csp_conf_get_defaults(&csp_config);
	csp_config.address = addr;
	csp_config.hostname = "satctl";
	csp_config.model = "linux";
	if (csp_init(&csp_config) < 0)
		return -1;

	//csp_debug_set_level(4, 1);
	//csp_debug_set_level(5, 1);

	if (use_uart) {
		struct usart_conf usart_conf = {
				.device = uart_dev,
				.baudrate = uart_baud,
		};
		usart_init(&usart_conf);

		static csp_iface_t kiss_if;
		static csp_kiss_handle_t kiss_handle;
		void kiss_usart_putchar(char c) {
			usart_putc(c);
		}
		void kiss_usart_callback(uint8_t *buf, int len, void *pxTaskWoken) {
			csp_kiss_rx(&kiss_if, buf, len, pxTaskWoken);
		}
		usart_set_callback(kiss_usart_callback);
		csp_kiss_init(&kiss_if, &kiss_handle, kiss_usart_putchar, kiss_discard, "KISS");
		csp_route_set(CSP_DEFAULT_ROUTE, &kiss_if, CSP_NODE_MAC);
		printf("Using usart %s baud %u\n", uart_dev, uart_baud);
		char *cmd = "\nset kiss_mode 1\n";
		usart_putstr(cmd, strlen(cmd));
	}

	if (use_can) {
		csp_iface_t *can0 = csp_can_socketcan_init(can_dev, 1000000, 1);
		if (can0) {
			if (csp_route_set(CSP_DEFAULT_ROUTE, can0, CSP_NODE_MAC) < 0) {
				return -1;
			}
		}
		printf("Using can %s baud %u\n", can_dev, 1000000);
	}

	if (csp_route_start_task(0, 0) < 0)
		return -1;

	csp_rdp_set_opt(3, 10000, 5000, 1, 2000, 2);
	//csp_rdp_set_opt(10, 20000, 8000, 1, 5000, 9);

	if (strlen(udp_peer_ip) > 0) {
		static csp_iface_t udp_client_if;
		csp_if_udp_init(&udp_client_if, udp_peer_ip);
		csp_rtable_set(0, 0, &udp_client_if, CSP_NODE_MAC);
	}

	/* Read routing table from parameter system */
	extern param_t csp_rtable;
	char rtable[csp_rtable.array_size];
	param_get_string(&csp_rtable, rtable, csp_rtable.array_size);

	if (csp_rtable_check(rtable)) {
		csp_rtable_clear();
		csp_rtable_load(rtable);
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

		printf(" Copyright (c) 2019 Space Inventor ApS <info@space-inventor.com>\n");
		printf(" Copyright (c) 2014 Satlab ApS <satlab@satlab.com>\n\n");

		slash_loop(slash, SATCTL_PROMPT_GOOD, SATCTL_PROMPT_BAD);
	}

	slash_destroy(slash);

	return 0;
}
