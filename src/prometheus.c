/*
 * prometheus_exporter.c
 *
 *  Created on: Aug 29, 2018
 *      Author: johan
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "prometheus.h"

static pthread_t prometheus_tread;

static char prometheus_buf[10*1024*1024] = {0};
static int prometheus_buf_len = 0;
static int listen_fd;

void * prometheus_exporter(void * param) {

	CURLcode res;
 	curl_global_init(CURL_GLOBAL_DEFAULT);
 	CURL *curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL, "https://mainecoon.space-inventor.com:8428/api/v1/import/prometheus");
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);

	while(1) {

		usleep(10000);

		if (prometheus_buf_len == 0) {
			continue;
		}

		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, prometheus_buf);
		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
      		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}

		prometheus_clear();

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


