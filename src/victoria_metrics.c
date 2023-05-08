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
#include <csp/csp.h>

#include <slash/slash.h>
#include <slash/optparse.h>

#include <param/param.h>
#include <param/param_queue.h>

#include "param_sniffer.h"
#include "hk_param_sniffer.h"

#include "base64.h"

static pthread_t vm_push_thread;
int vm_started = 0;
extern int prometheus_started;

#define SERVER_PORT 8428
#define SERVER_PORT_AUTH 8427
#define BUFFER_SIZE 10*1024*1024

char *server_ip = NULL;
char buffer[BUFFER_SIZE];
size_t buffer_size = 0;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
char request[BUFFER_SIZE + 1024];
char local_buffer[BUFFER_SIZE];
char *username = NULL;
char *password = NULL;
int server_port = 0;
char *dfl_server = "127.0.0.1";

static void encode_basic_auth(const char *username, const char *password, char *encoded_auth) {
    char auth_str[128] = {0};
    snprintf(auth_str, sizeof(auth_str), "%s:%s", username, password);

    base64_encodestate state;
    base64_init_encodestate(&state);

    int encoded_len = base64_encode_block(auth_str, strlen(auth_str), encoded_auth, &state);
    encoded_len += base64_encode_blockend(encoded_auth + encoded_len, &state);
    if (encoded_len > 0 && encoded_auth[encoded_len - 1] == '\n') {
        encoded_auth[--encoded_len] = '\0';
    }
}

void * vm_push(void * arg) {
    int sockfd;
    struct sockaddr_in server_addr;

    char encoded_auth[256];

    printf("Started pushing to %s:%d\n",server_ip, server_port);

    encode_basic_auth(username, password, encoded_auth);

    while (1) {
        // Lock the buffer mutex
        pthread_mutex_lock(&buffer_mutex);

        // Copy buffer to local_buffer and get current buffer_size
        strncpy(local_buffer, buffer, BUFFER_SIZE);
        size_t local_buffer_size = buffer_size;

        // Unlock the buffer mutex
        pthread_mutex_unlock(&buffer_mutex);

        if(local_buffer_size == 0){
            sleep(1);
            continue;
        }

        // Create a socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket() error");
            continue;
        }

        // Configure server address
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

        // Connect to server
        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect() error");
            close(sockfd);
            continue;
        }

        const csp_conf_t *conf = csp_get_conf();

        // Prepare HTTP request
        snprintf(request, BUFFER_SIZE + 1024,
                 "POST /api/v1/import/prometheus?extra_label=hostname=%s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Authorization: Basic %s\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n"
                 "%s",
                 conf->hostname, server_ip, server_port, encoded_auth, local_buffer_size, local_buffer);

        printf("Request:\n%s\n", request);

        // Send HTTP request
        if (send(sockfd, request, strlen(request), 0) < 0) {
            perror("send() error");
            close(sockfd);
            continue;
        }

        char buf[1024];
        int res_len = recv(sockfd, buf, 1023, 0);
        if(res_len < 0){
            perror("recv");
            close(sockfd);
            continue;
        }
        buf[res_len] = '\0';
        printf("Response:\n%s\n", buf);

        // Close the socket
        close(sockfd);

        pthread_mutex_lock(&buffer_mutex);
        buffer_size = 0;
        pthread_mutex_unlock(&buffer_mutex);

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

static int vm_start_cmd(struct slash *slash) {

    if (vm_started) return SLASH_SUCCESS;

    int hk_node = 0;
    int logfile = 0;

    optparse_t * parser = optparse_new("vm start", "");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "hk_node", "NUM", 0, &hk_node, "Housekeeping node");
    optparse_add_int(parser, 'p', "server port", "NUM", 0, &server_port, "Overwrite dfl port");
	optparse_add_set(parser, 'l', "logfile", 1, &logfile, "Enable logging to param_sniffer.log");
    optparse_add_string(parser, 'u', "user", "STRING", &username, "Username for vmauth");
    optparse_add_string(parser, 's', "server", "STRING", &server_ip, "Server for victoria metrics");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (username) {
        password = getpass("Enter vmauth password: ");
        if (server_port == 0) {
        server_port = SERVER_PORT_AUTH;
        }
    } else {
        server_port = SERVER_PORT;
    }

    if (server_ip == NULL) {
        server_ip = dfl_server;
    }

    if(!prometheus_started){
        param_sniffer_init(logfile, hk_node);
    }
	pthread_create(&vm_push_thread, NULL, &vm_push, NULL);
    vm_started = 1;
    optparse_del(parser);

    return SLASH_SUCCESS;
}
slash_command_sub(vm, start, vm_start_cmd, "", "Start Victoria Metrics push thread");
