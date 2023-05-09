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
int vm_running = 0;

#define SERVER_PORT 8428
#define SERVER_PORT_AUTH 8427
#define BUFFER_SIZE 10*1024*1024

char *server_ip = NULL;
char buffer[BUFFER_SIZE];
size_t buffer_size = 0;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
// space for buffer + header + size_t
char request[BUFFER_SIZE + 1024 + 20];
char header[1024];
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

int send_req(char *req_buf, struct sockaddr_in server_addr, int return_response){
    int sockfd;
    int status_code = 0;


    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket() error");
        return -1;
    }

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() error");
        close(sockfd);
        return -1;
    }

    // printf("Request:\n%s\n", req_buf);

    // Send HTTP request
    if (send(sockfd, req_buf, strlen(req_buf), 0) < 0) {
        perror("send() error");
        close(sockfd);
        return -1;
    }

    if(return_response){
        char res_buf[1024];
        int res_len = recv(sockfd, res_buf, 1023, 0);
        if (res_len < 0) {
          perror("recv");
          close(sockfd);
          return -1;
        }
        res_buf[res_len] = '\0';
        sscanf(res_buf, "%*s %d", &status_code);
        // printf("Response:\n%s\n", res_buf);
    }

    // printf("res code: %d\n", status_code);

    // Close the socket
    close(sockfd);

    return status_code;
}

void * vm_push(void * arg) {

    // Configure server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);


    // Just send basic auth header regardless
    char encoded_auth[256];
    if(password && username){
        encode_basic_auth(username, password, encoded_auth);
    }else{
        strncpy(encoded_auth, "none", 256);
    }
    const csp_conf_t *conf = csp_get_conf();

    char test_con[512];
    char query[] = "query=test42";
    snprintf(test_con, 512,
             "POST /prometheus/api/v1/query HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Authorization: Basic %s\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Content-Length: %ld\r\n\r\n%s",
              server_ip, server_port, encoded_auth, strlen(query), query);

    int res = send_req(test_con, server_addr, 1);
    if(res != 200 && res != 204){
        printf("Connection failed to %s:%d with code: %d\nTry again\n", server_ip, server_port, res);
        vm_running = 0;
    }else{
        printf("Started pushing to %s:%d\n",server_ip, server_port);
        // Prepare header without Content-Length
        snprintf(header, 1024,
                "POST /api/v1/import/prometheus?extra_label=hostname=%s HTTP/1.1\r\n"
                "Host: %s:%d\r\n"
                "Authorization: Basic %s\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: ",
                conf->hostname, server_ip, server_port, encoded_auth);
    }


    while (vm_running) {
        // Lock the buffer mutex
        pthread_mutex_lock(&buffer_mutex);
        if(buffer_size == 0){
            pthread_mutex_unlock(&buffer_mutex);
            sleep(1);
            continue;
        }
        // Build request buffer
        snprintf(request, BUFFER_SIZE + 1024 + 20, "%s%zu\r\n\r\n%s", header, buffer_size, buffer);
        // Unlock the buffer mutex
        pthread_mutex_unlock(&buffer_mutex);

        send_req(request, server_addr, 0);

        pthread_mutex_lock(&buffer_mutex);
        buffer_size = 0;
        pthread_mutex_unlock(&buffer_mutex);

        sleep(1);
    }

    // Clean up
    if(username){
        free(username);
        username = NULL;
    }
    if (server_ip && server_ip != dfl_server) {
        free(server_ip);
        server_ip = NULL;
    }
    server_port = 0;
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

    if (vm_running) return SLASH_SUCCESS;

    int hk_node = 0;
    int logfile = 0;
    char *tmp_username = NULL;
    char *tmp_server_ip = NULL;

    optparse_t * parser = optparse_new("vm start", "");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "hk_node", "NUM", 0, &hk_node, "Housekeeping node");
    optparse_add_int(parser, 'p', "server port", "NUM", 0, &server_port, "Overwrite dfl port");
    optparse_add_set(parser, 'l', "logfile", 1, &logfile, "Enable logging to param_sniffer.log");
    optparse_add_string(parser, 'u', "user", "STRING", &tmp_username, "Username for vmauth");
    optparse_add_string(parser, 's', "server", "STRING", &tmp_server_ip, "Server for victoria metrics");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (tmp_username) {
        username = strdup(tmp_username);
        password = getpass("Enter vmauth password: ");
        if (server_port == 0) {
            server_port = SERVER_PORT_AUTH;
        }
    } else if (server_port == 0) {
        server_port = SERVER_PORT;
    }

    if (tmp_server_ip == NULL) {
        server_ip = dfl_server;
    } else {
        server_ip = strdup(tmp_server_ip);
    }

    // if param_sniffer_init has already been called you can not change/set hk_node or logfile args
    param_sniffer_init(logfile, hk_node);
	pthread_create(&vm_push_thread, NULL, &vm_push, NULL);
    vm_running = 1;
    optparse_del(parser);

    return SLASH_SUCCESS;
}
slash_command_sub(vm, start, vm_start_cmd, "", "Start Victoria Metrics push thread");

static int vm_stop_cmd(struct slash *slash) {

    if (!vm_running) return SLASH_SUCCESS;

    vm_running = 0;

    return SLASH_SUCCESS;
}
slash_command_sub(vm, stop, vm_stop_cmd, "", "Stop Victoria Metrics push thread");
