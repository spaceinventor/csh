/*
 * victoria_metrics.c
 *
 *  Created on: May 4, 2023
 *      Author: Edvard
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include <slash/slash.h>
#include <slash/optparse.h>

#include <param/param.h>
#include <param/param_queue.h>

#include "param_sniffer.h"
#include "hk_param_sniffer.h"

static pthread_t vm_push_thread;

#define SERVER_PORT 8428
#define BUFFER_SIZE 1024

char *server_ip;
char buffer[BUFFER_SIZE];
size_t buffer_size = 0;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void * vm_push(void * arg) {
    int sockfd;
    struct sockaddr_in server_addr;
    char request[BUFFER_SIZE * 2];
    char local_buffer[BUFFER_SIZE];

    while (1) {
        // Lock the buffer mutex
        pthread_mutex_lock(&buffer_mutex);

        // Copy buffer to local_buffer and get current buffer_size
        strncpy(local_buffer, buffer, BUFFER_SIZE);
        size_t local_buffer_size = buffer_size;

        // Unlock the buffer mutex
        pthread_mutex_unlock(&buffer_mutex);

        // continue if local_buffer_size = 0 TODO

        // Create a socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket() error");
            continue;
        }

        // Configure server address
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

        // Connect to server
        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect() error");
            close(sockfd);
            continue;
        }

        // Prepare HTTP request
        snprintf(request, BUFFER_SIZE * 2,
                 "POST /api/v1/import/prometheus HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n"
                 "%s",
                 server_ip, SERVER_PORT, local_buffer_size, local_buffer);

        // Send HTTP request
        if (send(sockfd, request, strlen(request), 0) < 0) {
            perror("send() error");
            close(sockfd);
            continue;
        }
        // clear buffer and length TODO

        // Close the socket
        close(sockfd);

        // Sleep for 1 second
        sleep(1);
    }

    return NULL;
}

void vm_add(char * metric_line) {
    // Lock the buffer mutex
    pthread_mutex_lock(&buffer_mutex);

    // Check if there's enough space in the buffer
    size_t line_len = strlen(metric_line);
    if (buffer_size + line_len < BUFFER_SIZE) {
        // Add the new metric line to the buffer
        strncpy(buffer + buffer_size, metric_line, BUFFER_SIZE - buffer_size);
        buffer_size += line_len;
    }

    // Unlock the buffer mutex
    pthread_mutex_unlock(&buffer_mutex);
}

void vm_init(void) {
	pthread_create(&vm_push_thread, NULL, &vm_push, NULL);
}


static int vm_start_cmd(struct slash *slash) {

	int hk_node = 0;
	int logfile = 0;

    optparse_t * parser = optparse_new("record", "");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "hk_node", "NUM", 0, &hk_node, "Housekeeping node");
	optparse_add_set(parser, 'l', "logfile", 1, &logfile, "Enable logging to param_sniffer.log");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("Missing label name\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
	char * name = slash->argv[argi];

    // string copy this TODO
    // create global variable for wether we are running prometheus or victoria
    // TODO mutate the str passed from param_sniffer to include label
    if (++argi >= slash->argc) {
        printf("No server using 127.0.0.1\n");
        server_ip = "127.0.0.1";
    } else {
        server_ip = slash->argv[argi];
    }

    vm_init();
    param_sniffer_init(logfile, hk_node);

    return SLASH_SUCCESS;
}
slash_command(record, vm_start_cmd, "<name> [server]", "Start Victoria Metrics push thread")
