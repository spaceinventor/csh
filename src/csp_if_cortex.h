#pragma once

#include <csp/csp.h>

#include <pthread.h>
#include <netinet/in.h>

typedef struct {

	/* Should be set before calling if_cortex_init */
	char * host;
	int rx_port;
	int tx_port;
    struct sockaddr_in cortex_ip;

	/* Internal parameters */
	pthread_t rx_task;
    pthread_t parser_task;
	int sockfd_rx;
    int sockfd_tx;

} csp_if_cortex_conf_t;

void csp_if_cortex_init(csp_iface_t * iface, csp_if_cortex_conf_t * ifconf);
