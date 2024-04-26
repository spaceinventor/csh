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

#include <slash/slash.h>
#include <slash/optparse.h>

#include <param/param.h>
#include <param/param_queue.h>

#include "prometheus.h"
#include "param_sniffer.h"

static pthread_t prometheus_tread;
int prometheus_started = 0;

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
		int bread = recv(conn_fd, in, 1024, MSG_NOSIGNAL);
		if (bread == 0) {
			close(conn_fd);
			continue;
		}

		if (strncmp(in, "GET /metrics", 12) != 0) {
			close(conn_fd);
			continue;
		}

		int written = send(conn_fd, header, strlen(header), MSG_NOSIGNAL);

		/* Dump queued data */
		written += send(conn_fd, prometheus_buf, prometheus_buf_len, MSG_NOSIGNAL);
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
	printf("prometheus add %s", str);
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


static int prometheus_start_cmd(slash_t *slash) {

    if(prometheus_started) return SLASH_SUCCESS;

    int logfile = 0;

    optparse_t * parser = optparse_new("prometheus start", "");
    optparse_add_help(parser);
    optparse_add_set(parser, 'l', "logfile", 1, &logfile, "Enable logging to param_sniffer.log");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        return SLASH_EINVAL;
        optparse_del(parser);
    }

    prometheus_init();
    param_sniffer_init(logfile);
    prometheus_started = 1;
    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command_sub(prometheus, start, prometheus_start_cmd, NULL, "Start prometheus webserver");
