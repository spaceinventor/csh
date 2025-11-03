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
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include <slash/slash.h>
#include <slash/optparse.h>

#include <csp/csp.h>
#include <param/param_queue.h>
#include <param/param_string.h>
#include "param_sniffer.h"
#include "url_utils.h"

static pthread_t vm_push_thread;
int vm_running = 0;

#define SERVER_PORT      8428
#define SERVER_PORT_AUTH 8427
#define BUFFER_SIZE      10 * 1024 * 1024

static char buffer[BUFFER_SIZE];
static size_t buffer_size = 0;
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int use_ssl;
    int port;
    int skip_verify;
    int verbose;
    char * api_root;
    char * username;
    char * password;
    char * server_ip;
} vm_args;

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    return size * nmemb;
}

void * vm_push(void * arg) {

    vm_args * args = arg;

    CURL * curl;
    CURLcode res;
    struct curl_slist * headers = NULL;

    curl = curl_easy_init();
    const char * hostname = csp_get_conf()->hostname;
    char url[256];
    char protocol[8] = "http";

    if (args->use_ssl) {
        protocol[4] = 's';
    }

    if (curl) {
        if (args->skip_verify) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        if (args->verbose) {
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        }else
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);


        if (args->username && args->password) {
            curl_easy_setopt(curl, CURLOPT_USERNAME, args->username);
            curl_easy_setopt(curl, CURLOPT_PASSWORD, args->password);
            curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        }

        // Test connection
        if(args->api_root) {
            if(args->api_root[strlen(args->api_root) - 1] == '/') {
                snprintf(url, sizeof(url), "%sprometheus/api/v1/query", args->api_root);
            } else {
                snprintf(url, sizeof(url), "%s/prometheus/api/v1/query", args->api_root);
            }
        } else {
            snprintf(url, sizeof(url), "%s://%s:%d/prometheus/api/v1/query", protocol, args->server_ip, args->port);
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "query=test42");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 12);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("Failed test of connection: %s\n", curl_easy_strerror(res));
            vm_running = 0;
        }
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 200 && res == CURLE_OK) {
            printf("Failed test with response code: %ld\n", response_code);
            vm_running = 0;
        }

        // Resume building of header for push
        if(args->api_root) {
            if(args->api_root[strlen(args->api_root) - 1] == '/') {
                snprintf(url, sizeof(url), "%sapi/v1/import/prometheus?extra_label=instance=%s", args->api_root, hostname);
            } else {
                snprintf(url, sizeof(url), "%s/api/v1/import/prometheus?extra_label=instance=%s", args->api_root, hostname);
            }
        } else {
            snprintf(url, sizeof(url), "%s://%s:%d/api/v1/import/prometheus?extra_label=instance=%s", protocol, args->server_ip, args->port, hostname);
        }
        curl_easy_setopt(curl, CURLOPT_URL, url);
        headers = curl_slist_append(headers, "Content-Type: text/plain");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        if (args->verbose) {
            printf("Full URL: %s\n", url);
        }

    } else {
        vm_running = 0;
        printf("curl_easy_init() failed\n");
    }

    if (vm_running) {
        if(args->api_root) {
            printf("Connection established to %s", args->api_root);
        } else {
            printf("Connection established to %s://%s:%d\n", protocol, args->server_ip, args->port);
        }
    }

    while (vm_running) {
        // Lock the buffer mutex
        pthread_mutex_lock(&buffer_mutex);
        if (buffer_size == 0) {
            pthread_mutex_unlock(&buffer_mutex);
            sleep(1);
            continue;
        }

        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, buffer_size);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("Failed push: %s\n", curl_easy_strerror(res));
        } else {
            buffer_size = 0;
        }
        // Unlock the buffer mutex
        pthread_mutex_unlock(&buffer_mutex);

        sleep(1);
    }

    printf("vm push stopped\n");
    // Clean up
    if (curl) {
        curl_easy_cleanup(curl);
    }
    if (headers) {
        curl_slist_free_all(headers);
    }
    if (args->username) {
        free(args->username);
        args->username = NULL;
    }
    if (args->password) {
        free(args->password);
        args->password = NULL;
    }
    if (args->server_ip) {
        free(args->server_ip);
        args->server_ip = NULL;
    }
    if (args->api_root) {
        free(args->api_root);
        args->api_root = NULL;
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
        strcpy(buffer + buffer_size, metric_line);
        buffer_size += line_len;
    }

    // Unlock the buffer mutex
    pthread_mutex_unlock(&buffer_mutex);
}

void vm_add_param(param_t * param) {

    if(param->type == PARAM_TYPE_STRING || param->type == PARAM_TYPE_DATA){
        return;
    }
    static char outstr[1000] = {};
    static char valstr[100] = {};
    int arr_cnt = param->array_size;
    if (arr_cnt < 0)
        arr_cnt = 1;

    struct timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t time_ms = ((uint64_t) tv.tv_sec * 1000000 + tv.tv_usec) / 1000;

    for (int j = 0; j < arr_cnt; j++) {
        param_value_str(param, j, valstr, 100);
        snprintf(outstr, 1000, "%s{node=\"%u\", idx=\"%u\"} %s %"PRIu64"\n", param->name, *(param->node), j, valstr, time_ms);
        vm_add(outstr);
    }
}

static vm_args victoria_metrics_args = { 0 };

static int vm_start_cmd(struct slash * slash) {

    if (vm_running) return SLASH_SUCCESS;

    int logfile = 0;
    char * tmp_username = NULL;
    char * tmp_password = NULL;
    char * key_file = NULL;
    char * sec_key = NULL;
    char * api_root = NULL;
    vm_args * args = &victoria_metrics_args;

    optparse_t * parser = optparse_new("vm start", "<server or full HTTP(s) API root for the targetted Victoria Metrics instance>");
    optparse_add_help(parser);
    optparse_add_string(parser, 'u', "user", "STRING", &tmp_username, "Username for vmauth");
    optparse_add_string(parser, 'p', "pass", "STRING", &tmp_password, "Password for vmauth");
    optparse_add_string(parser, 'a', "auth_file", "STR", &key_file, "file containing private key for zmqproxy (default: None)");
    optparse_add_set(parser, 's', "ssl", 1, &(args->use_ssl), "Use SSL/TLS");
    optparse_add_int(parser, 'P', "server-port", "NUM", 0, &(args->port), "Overwrite default port");
    optparse_add_set(parser, 'l', "logfile", 1, &logfile, "Enable logging to param_sniffer.log");
    optparse_add_set(parser, 'S', "skip-verify", 1, &(args->skip_verify), "Skip verification of the server's cert and hostname");
    optparse_add_set(parser, 'v', "verbose", 1, &(args->verbose), "Verbose connect");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    if(!api_root) {
        if (++argi >= slash->argc) {
            printf("Missing server ip/domain\n");
            optparse_del(parser);
            return SLASH_EINVAL;
        }
    }

    if(false == is_http_url(slash->argv[argi])) {
        args->server_ip = strdup(slash->argv[argi]);
        args->api_root = NULL;
    } else {
        args->api_root = strdup(slash->argv[argi]);
        args->server_ip = NULL;
    }

    if(key_file) {

        char key_file_local[256];
        if (key_file[0] == '~') {
            strcpy(key_file_local, getenv("HOME"));
            strcpy(&key_file_local[strlen(key_file_local)], &key_file[1]);
        }
        else {
            strcpy(key_file_local, key_file);
        }

        FILE *file = fopen(key_file_local, "r");
        if(file == NULL){
            printf("Could not open config %s\n", key_file_local);
            optparse_del(parser);
            return SLASH_EINVAL;
        }

        sec_key = malloc(1024);
        if (sec_key == NULL) {
            printf("Failed to allocate memory for secret key.\n");
            fclose(file);
            optparse_del(parser);
            return SLASH_EINVAL;
        }

        if (fgets(sec_key, 1024, file) == NULL) {
            printf("Failed to read secret key from file.\n");
            free(sec_key);
            fclose(file);
            optparse_del(parser);
            return SLASH_EINVAL;
        }
        fclose(file);
    }

    if (tmp_username) {
        if (!tmp_password && !sec_key) {
            printf("Provide password with -p\n");
            optparse_del(parser);
            return SLASH_EINVAL;
        }
        args->username = strdup(tmp_username);
        args->password = strdup(sec_key ? sec_key : tmp_password);
        char * const newline = strchr(args->password, '\n');
        if (newline) {
            *newline = '\0';
        }
        if (!args->port) {
            args->port = SERVER_PORT_AUTH;
        }
    } else if (!args->port) {
        args->port = SERVER_PORT;
    }

    param_sniffer_init(logfile);
    pthread_create(&vm_push_thread, NULL, &vm_push, args);
    vm_running = 1;
    optparse_del(parser);

    if (sec_key != NULL) {
        free(sec_key);
    }
    return SLASH_SUCCESS;
}
slash_command_sub(vm, start, vm_start_cmd, "", "Start Victoria Metrics push thread");

static int vm_stop_cmd(struct slash * slash) {

    vm_args * args = &victoria_metrics_args;
    if (!vm_running) return SLASH_SUCCESS;

    if(args->username) {
        free(args->username);
        args->username = NULL;
    }
    if(args->password) {
         free(args->password);
         args->password = NULL;
    }
    if(args->api_root) {
         free(args->api_root);
         args->api_root = NULL;
    }
    if(args->server_ip) {
         free(args->server_ip);
         args->server_ip = NULL;
    }

    vm_running = 0;

    return SLASH_SUCCESS;
}
slash_command_sub(vm, stop, vm_stop_cmd, "", "Stop Victoria Metrics push thread");
