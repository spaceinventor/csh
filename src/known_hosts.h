#pragma once

#include <stdbool.h>

#include "libparam.h"

#define HOSTNAME_MAXLEN 50

#include <sys/queue.h>

typedef struct host_s {
    int node;
    char name[HOSTNAME_MAXLEN];
    SLIST_ENTRY(host_s) next;
} host_t;

host_t * known_hosts_add(int addr, const char * new_name, bool override_existing);
int known_hosts_get_name(int find_host, char * name, int buflen);
int known_hosts_get_node(const char * find_name);