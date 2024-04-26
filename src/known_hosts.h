#pragma once

#include <stdbool.h>

#include "libparam.h"

#define HOSTNAME_MAXLEN 50

#ifndef PARAM_HAVE_SYS_QUEUE
#error "Known Hosts cannot work without PARAM_HAVE_SYS_QUEUE, and there's currently no guarantee that it disables cleanly without it."
#endif

#ifdef PARAM_HAVE_SYS_QUEUE
#include <sys/queue.h>
#endif

typedef struct host_s {
    int node;
    char name[HOSTNAME_MAXLEN];
#ifdef PARAM_HAVE_SYS_QUEUE
	/* single linked list:
	 * The weird definition format comes from sys/queue.h SLINST_ENTRY() macro */
	struct { struct host_s *sle_next; } next;
#endif
} host_t;

host_t * known_hosts_add(int addr, const char * new_name, bool override_existing);
int known_hosts_get_name(int find_host, char * name, int buflen);
int known_hosts_get_node(const char * find_name);