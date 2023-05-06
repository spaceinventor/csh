#pragma once

#include <csp/csp.h>

#include <pthread.h>
#include <netinet/in.h>

typedef struct {

	/* Should be set before calling if_cortex_init */
	char * host;
	int rx_port;
	int tx_port;

	/* Internal parameters */
    struct sockaddr_in cortex_ip;
	pthread_t rx_task;
    pthread_t parser_task;

} csp_if_cortex_conf_t;

void csp_if_cortex_init(csp_iface_t * iface, csp_if_cortex_conf_t * ifconf);
