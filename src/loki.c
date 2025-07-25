#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <apm/csh_api.h>

#include <csp/csp.h>
#include <csp/csp_types.h>
#include <csp/arch/csp_queue.h>

#include "arch/posix/pthread_queue.h"
#include "url_utils.h"

static int loki_running = 0;

#define SERVER_PORT      3100
#define BUFFER_SIZE      1024 * 1024

static char readbuffer[BUFFER_SIZE] = {0};
static char formatted_log[BUFFER_SIZE - 100] = {0};
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_queue_t * loki_q;

typedef struct {
    char * data;
    size_t len;
} json_str_t;


int pipe_fd[2]; 
static int old_stdout;

typedef struct {
    int use_ssl;
    int port;
    int skip_verify;
    int verbose;
    char * username;
    char * password;
    char * server_ip;
    char * api_root;
} loki_args;

static loki_args args = {0};
static CURL * curl;
static CURLcode res;
struct curl_slist * headers = NULL;
static char url[256];
static char protocol[8] = "http";
static int curl_err_count = 0;

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    return size * nmemb;
}

/* strip ansi escape codes, special characters and newlines to nullterminator
 * returns if number of nulltermintors inserted
*/
int strip_str(char *str) {
    char *read_p = str;
    char *write_p = str;
    int str_counter = 0;

    // remove if starting with newline or char return for ident and stdbuf2
    while(*read_p == '\n' || *read_p == '\r'){
        read_p++;
    }

    while (*read_p) {
        if (*read_p == 27) { // Check for the escape character
            read_p++;  // move over the escape character

            // check for the opening square bracket '[' after the escape character
            if (*read_p == '[') {
                read_p++;

                // skip over numbers and semicolons
                while ((*read_p >= '0' && *read_p <= '9') || *read_p == ';') {
                    read_p++;
                }

                // move over the final character of the sequence (like 'm' in most cases)
                if (*read_p != '\0') {
                    read_p++;
                }
            }
        } else if (*read_p == '\n') {
            // skip continous special characters
            while (*read_p && (*read_p < 32) && *read_p != 27) {
                read_p++;
            }
            *write_p = '\0';
            write_p++;
            str_counter++;
        } else if (*read_p == '\t'){
            *write_p = ' ';
            read_p++;
            write_p++;
        } else if (*read_p == '"'){
            *write_p = '\'';
            read_p++;
            write_p++;
        } else if (*read_p < 32) {
            read_p++;
        } else {
            // Copy any other character
            *write_p = *read_p;
            write_p++;
            read_p++;
        }
    }

// TODO hope we get saved by the double \0\0 in the build json loop
// this makes stdbuf2 which prints chunks of data where a newline is not part of the last line sometimes work
    if(str_counter >= 2){
        str_counter++;
    }

    // Null terminate the modified string
    *write_p = '\0';
    return str_counter;
}

void loki_add(char * log, int iscmd) {
    if(!loki_running){
        return;
    }

    int str_counter = strip_str(log);

    if(str_counter < 1){
        return;
    }

    // Lock the buffer mutex
    pthread_mutex_lock(&buffer_mutex);

    char *p = log;
    formatted_log[0] = '\0';
    int i = 0;
    while(*p){
        // filter empty cmd line we get in stdout pipe this could hit stdouts you actually want
        // we still get empty cmds logged as no timestamp is added
        if(strstr(p, " @ ") && strstr(p, " d. ")){
            goto next;
        }

        struct timeval tv;
        gettimeofday(&tv, NULL);
        long long msec = (long long)(tv.tv_sec) * 1000000 + tv.tv_usec;
        char temp[4048];
        snprintf(temp, sizeof(temp), "[\"%lld000\", \"%s\"],", msec, p);
        strcat(formatted_log, temp);

next:
        // Move to the next log line
        p += strlen(p) + 1;  
        i++;
        // make sure we dont just too far
        if(i == str_counter){
            break;
        }
    }
    if(i == 0){
        pthread_mutex_unlock(&buffer_mutex);
        return;
    }

    int format_len = 0; 
    if(*formatted_log){
        format_len = strlen(formatted_log);
        formatted_log[format_len - 1] = '\0';
    } else {
        pthread_mutex_unlock(&buffer_mutex);
        return;
    }

    const char json_format_str[] = "{"
    "\"streams\": [{"
    "\"stream\": {"
    "\"instance\": \"%s\","
    "\"node\": \"%u\","
    "\"type\": \"%s\""
    "},"
    "\"values\": [%s]"
    "}]"
    "}";

    const size_t json_str_max = format_len + CSP_HOSTNAME_LEN + sizeof(json_format_str) + 11; // 11 for type & node
    char * json_str_buf = malloc(json_str_max);
    if(!json_str_buf){
        const char err_str[] = "\033[1;31mLOKI CURL: Out of memory! Log skipped\033[0m\n";
        size_t written = write(old_stdout, err_str, sizeof(err_str));
        (void)written;
        pthread_mutex_unlock(&buffer_mutex);
        return;
    }

    int written = snprintf(json_str_buf, json_str_max, json_format_str, csp_get_conf()->hostname, slash_dfl_node, iscmd ? "cmd" : "stdout", formatted_log);

    pthread_mutex_unlock(&buffer_mutex);
    json_str_t json_str = {.data = json_str_buf, .len = written};
    pthread_queue_enqueue(loki_q, &json_str, CSP_MAX_TIMEOUT);
}

static void *post_thread(void *arg) {

    while(loki_running){
        json_str_t json_str;
        int p_res = pthread_queue_dequeue(loki_q, (void *)&json_str, CSP_MAX_TIMEOUT);
        if(p_res != PTHREAD_QUEUE_OK){
            continue;
        }
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_str.len);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.data);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK){
            curl_err_count++;
            char res_str[512];
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (0 != response_code) {
                // There is an HTTP response code to look at, so do that
                snprintf(res_str, 512, "\033[1;31mLOKI CURL: http response code was: %ld\033[0m\n", response_code);
            } else {
                // An error occured in such way that there is no HTTP response code at all
                snprintf(res_str, 512, "\033[1;31mLOKI CURL: %s\033[0m\n", curl_easy_strerror(res));
            }
            ssize_t wres = write(old_stdout, res_str, strlen(res_str));
            (void)wres;
            if(curl_err_count > 5){
                loki_running = 0;
                curl_err_count = 0;
                printf("\n\033[31mLOKI LOGGING STOPPED!\033[0m\n");
            }
        } else {
            curl_err_count = 0;
        }
        free(json_str.data);
    }
    return NULL;
}

static void *read_pipe(void *arg) {
    int n;

    while(loki_running){
        n = read(pipe_fd[0], readbuffer, sizeof(readbuffer) - 1);
        if(n > 0){
            ssize_t wres = write(old_stdout, readbuffer, n); // write to terminal
            (void)wres;
            loki_add(readbuffer, 0);
            memset(readbuffer, 0, n);
        }
    }
    close(pipe_fd[0]);
    dup2(old_stdout, 1);
    close(old_stdout);
    return NULL;
}

/* Log commands to a Loki instance if enabled */
void on_loki_slash_execute_hook(const char *line) {
	if(loki_running){
		int ex_len = strlen(line);
		char * dup = malloc(ex_len + 2);
        if(dup) {
            strncpy(dup, line, ex_len + 2);
            dup[ex_len] = '\n';
            dup[ex_len + 1] = '\0';
            loki_add(dup, 1);
            free(dup);
        }
	}
}

static int loki_start_cmd(struct slash * slash) {

    char * tmp_username = NULL;
    char * tmp_password = NULL;
    char * key_file = NULL;

    optparse_t * parser = optparse_new("loki start", "<server or full HTTP(s) API root for the targetted Loki instance>");
    optparse_add_help(parser);
    optparse_add_string(parser, 'u', "user", "STRING", &tmp_username, "Username for Loki logging");
    optparse_add_string(parser, 'p', "pass", "STRING", &tmp_password, "Password for Loki logging");
    optparse_add_string(parser, 'a', "auth_file", "STR", &key_file, "File containing private key for Loki logging (default: None)");
    optparse_add_set(parser, 's', "ssl", 1, &(args.use_ssl), "Use SSL/TLS");
    optparse_add_int(parser, 'P', "server-port", "NUM", 0, &(args.port), "Overwrite default port");
    optparse_add_set(parser, 'S', "skip-verify", 1, &(args.skip_verify), "Skip verification of the server's cert and hostname");
    optparse_add_set(parser, 'v', "verbose", 1, &(args.verbose), "Verbose connect");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    if (loki_running) {
        printf("Loki logging is already configured\n");
        optparse_del(parser);
        return SLASH_SUCCESS;
    }

    if (++argi >= slash->argc) {
        printf("Missing server/API root\n");
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    if(false == is_http_url(slash->argv[argi])) {
        args.server_ip = strdup(slash->argv[argi]);
        args.api_root = NULL;
    } else {
        args.api_root = strdup(slash->argv[argi]);
        args.server_ip = NULL;
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

        tmp_password = malloc(1024);
        if (tmp_password == NULL) {
            printf("Failed to allocate memory for secret key.\n");
            fclose(file);
            optparse_del(parser);
            return SLASH_EINVAL;
        }

        if (fgets(tmp_password, 1024, file) == NULL) {
            printf("Failed to read secret key from file.\n");
            free(tmp_password);
            fclose(file);
            optparse_del(parser);
            return SLASH_EINVAL;
        }
        tmp_password[strcspn(tmp_password, "\n")] = 0;
        fclose(file);
    }

    if (tmp_username) {
        if (!tmp_password) {
            printf("Provide password with -p\n");
            optparse_del(parser);
            return SLASH_EINVAL;
        }
        args.username = strdup(tmp_username);
        args.password = strdup(tmp_password);
        if (!args.port) {
            args.port = SERVER_PORT;
        }
    } else if (!args.port) {
        args.port = SERVER_PORT;
    }

    curl = curl_easy_init();

    if (args.use_ssl) {
        protocol[4] = 's';
    }

    if (curl) {
        if (args.skip_verify) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        if (args.verbose) {
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        }else
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);


        if (args.username && args.password) {
            curl_easy_setopt(curl, CURLOPT_USERNAME, args.username);
            curl_easy_setopt(curl, CURLOPT_PASSWORD, args.password);
            curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        }
        if(args.api_root != NULL) {
            if(args.api_root[strlen(args.api_root) - 1] == '/') {
                snprintf(url, sizeof(url), "%sloki/api/v1/push", args.api_root);
            } else {
                snprintf(url, sizeof(url), "%s/loki/api/v1/push", args.api_root);
            }
        } else {
            snprintf(url, sizeof(url), "%s://%s:%d/loki/api/v1/push", protocol, args.server_ip, args.port);
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        if (args.verbose) {
            printf("Full URL: %s\n", url);
        }

    } else {
        loki_running = 0;
        printf("curl_easy_init() failed\n");
    }

    // TODO create a test GET on http://localhost:3100/ready to check connection
    // if (loki_running) {
    //     printf("Connection established to %s://%s:%d\n", protocol, args.server_ip, args.port);
    // }

    // pthread_create(&loki_thread, NULL, &loki_push, args);

    if (pipe(pipe_fd) < 0) {
        perror("pipe");
        return SLASH_EINVAL;
    }

    old_stdout = dup(fileno(stdout));
    if (dup2(pipe_fd[1], fileno(stdout)) == -1) {
        perror("dup2");
        return SLASH_EINVAL;
    }
    if(!loki_q){
        loki_q = pthread_queue_create(2000, sizeof(json_str_t));
        if(!loki_q){
            printf("\033[1;31mLOKI CURL: Out of memory!\033[0m\n");
            return SLASH_EINVAL;
        }

    }
    loki_running = 1;
    pthread_t read_thread_id;
    pthread_create(&read_thread_id, NULL, &read_pipe, NULL);
    pthread_t post_thread_id;
    pthread_create(&post_thread_id, NULL, &post_thread, NULL);
    printf("Loki logging started\n");

    optparse_del(parser);

    return SLASH_SUCCESS;
}
slash_command_sub(loki, start, loki_start_cmd, "", "Start Loki log push thread");
