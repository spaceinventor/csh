/*
 * prometheus_exporter.c
 *
 *  Created on: Aug 29, 2018
 *      Author: johan
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "prometheus.h"

static pthread_t prometheus_tread;

static char prometheus_buf[10*1024*1024] = {0};
static int prometheus_buf_len = 0;
static int listen_fd;

static char header[1024] =
		"HTTP/1.1 200 OK;\r\n"
		"Content-Type: text/plain;\r\n\r\n";


void * prometheus_exporter(void * param) {

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    	printf("setsockopt(SO_REUSEADDR) failed\n");

	struct sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(9101);

retry_bind:
	if (bind(listen_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		printf("Cannot bind prometheus to port 9101\n");
		sleep(1);
		goto retry_bind;
	} else {
		printf("Prometheus exporter listening on port 9101\n");
	}

	listen(listen_fd, 100);

	while(1) {

		struct sockaddr_in client_addr;
		socklen_t client_addr_len;
		int conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);

		char in[1024];
		int bread = read(conn_fd, in, 1024);
		if (bread == 0) {
			close(conn_fd);
			continue;
		}

		if (strncmp(in, "GET /metrics", 12) != 0) {
			close(conn_fd);
			continue;
		}

		int written = write(conn_fd, header, strlen(header));

		/* Dump queued data */
		written += write(conn_fd, prometheus_buf, prometheus_buf_len);
		//printf("Prometheus sent %d bytes\n", written);

		prometheus_clear();

		shutdown(conn_fd, SHUT_RDWR);

		close(conn_fd);

	}

	return NULL;

}

void prometheus_add(char * str) {
	if (prometheus_buf_len + strlen(str) < 1024 * 1024 * 10) {
		strcpy(prometheus_buf + prometheus_buf_len, str);
		prometheus_buf_len += strlen(str);
	}
	//printf("prometheus add %s, buflen %d\n", str, prometheus_buf_len);
}

void prometheus_clear(void) {
	prometheus_buf_len = 0;
}

void prometheus_init(void) {
	pthread_create(&prometheus_tread, NULL, &prometheus_exporter, NULL);
}

void prometheus_close(void) {
	close(listen_fd);
}


