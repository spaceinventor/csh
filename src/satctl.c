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
#include <param/rparam.h>

#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/drivers/usart.h>

#define SATCTL_PROMPT_GOOD		"\033[96msatctl \033[90m%\033[0m "
#define SATCTL_PROMPT_BAD		"\033[96msatctl \033[31m!\033[0m "
#define SATCTL_DEFAULT_INTERFACE	"can0"
#define SATCTL_DEFAULT_ADDRESS		1

#define SATCTL_LINE_SIZE		128
#define SATCTL_HISTORY_SIZE		2048

static uint32_t u32_data;
static int32_t i32_data;
static uint8_t u8_data;
static float flt_data;
static char str_data[100] = "hest";

PARAM_DEFINE_STATIC_RAM(u32, PARAM_TYPE_UINT32, -1, 0, UINT32_MAX, PARAM_READONLY_FALSE, NULL, "", &u32_data);
PARAM_DEFINE_STATIC_RAM(i32, PARAM_TYPE_INT32, -1, INT32_MIN, INT32_MAX, PARAM_READONLY_FALSE, NULL, "", &i32_data);
PARAM_DEFINE_STATIC_RAM(u8, PARAM_TYPE_UINT8, -1, 0, UINT8_MAX, PARAM_READONLY_FALSE, NULL, "", &u8_data);
PARAM_DEFINE_STATIC_RAM(flt, PARAM_TYPE_FLOAT, -1, -1, -1, PARAM_READONLY_FALSE, NULL, "", &flt_data);
PARAM_DEFINE_STATIC_RAM(str, PARAM_TYPE_STRING, 100, -1, -1, PARAM_READONLY_FALSE, NULL, "", str_data);


void usage(void)
{
	printf("usage: satctl [command]\n");
	printf("\n");
	printf("Satlab Control Application v" SATCTL_VERSION ".\n");
	printf("\n");
	printf("Copyright (c) 2014 Satlab ApS <satlab@satlab.com>\n");
	printf("\n");
	printf("Options:\n");
	printf(" -i INTERFACE,\tUse INTERFACE as CAN interface\n");
	printf(" -m NODE\tUse NODE as own CSP addres\n");
	printf(" -h,\t\tPrint this help and exit\n");
}

int configure_csp(uint8_t addr, char *ifc)
{
	struct csp_can_config can_conf = {
		.ifc = ifc
	};

	if (csp_buffer_init(25, 320) < 0)
		return -1;

	csp_set_hostname("satctl");
	csp_set_model("linux");

	if (csp_init(addr) < 0)
		return -1;

#if 0
	struct usart_conf usart_conf = {
			.device = "/dev/ttyUSB0",
			.baudrate = 38400,
	};
	usart_init(&usart_conf);

	static csp_iface_t kiss_if;
	static csp_kiss_handle_t kiss_handle;
	void kiss_usart_putchar(char c) {
		usleep(4000);
		usart_putc(c);
	}
	void kiss_usart_callback(uint8_t *buf, int len, void *pxTaskWoken) {
		csp_kiss_rx(&kiss_if, buf, len, pxTaskWoken);
	}
	usart_set_callback(kiss_usart_callback);
	csp_kiss_init(&kiss_if, &kiss_handle, kiss_usart_putchar, NULL, "KISS");
#endif

	if (csp_can_init(CSP_CAN_MASKED, &can_conf) < 0)
		;/* return -1; */

	if (csp_route_set(CSP_DEFAULT_ROUTE, &csp_if_can, CSP_NODE_MAC) < 0)
		return -1;

	if (csp_route_start_task(0, 0) < 0)
		return -1;

	csp_thread_handle_t server_handle;
	csp_thread_create(rparam_server_task, "param", 2000, NULL, 1, &server_handle);

	return 0;
}

int main(int argc, char **argv)
{
	struct slash *slash;
	int remain, index, i, c, p = 0;
	char *ex;

	uint8_t addr = SATCTL_DEFAULT_ADDRESS;
	char *ifc = SATCTL_DEFAULT_INTERFACE;

	while ((c = getopt(argc, argv, "+hi:n:")) != -1) {
		switch (c) {
		case 'i':
			ifc = optarg;
			break;
		case 'n':
			addr = atoi(optarg);
			break;
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case '?':
			if (optopt == 'c')
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
		default:
			exit(EXIT_FAILURE);
		}
	}

	remain = argc - optind;
	index = optind;

	if (configure_csp(addr, ifc) < 0) {
		fprintf(stderr, "Failed to init CSP\n");
		exit(EXIT_FAILURE);
	}

	slash = slash_create(SATCTL_LINE_SIZE, SATCTL_HISTORY_SIZE);
	if (!slash) {
		fprintf(stderr, "Failed to init slash\n");
		exit(EXIT_FAILURE);
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
		printf("Satlab Control v" SATCTL_VERSION "\n");
		printf("Copyright (c) 2014-2016 Satlab ApS <satlab@satlab.com>\n\n");

		slash_loop(slash, SATCTL_PROMPT_GOOD, SATCTL_PROMPT_BAD);
	}

	slash_destroy(slash);

	return 0;
}
