/**
 * Naive, slow and simple storage of nodeid and hostname
 */

 #include "known_hosts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <sys/queue.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <apm/csh_api.h>

#define HOSTNAME_MAXLEN 50

struct host_s {
    int node;
    char name[HOSTNAME_MAXLEN];
    SLIST_ENTRY(host_s) next;
};

static uint32_t known_host_storage_size = sizeof(host_t);
SLIST_HEAD(known_host_s, host_s) known_hosts = {};

/** Private (CSH-only API) */
void node_save(const char * filename) {
    FILE * out = stdout;
    if (filename) {
        FILE * fd = fopen(filename, "w");
        if (fd) {
            out = fd;
            printf("Writing to file %s\n", filename);
        }
    }

    for (host_t* host = SLIST_FIRST(&known_hosts); host != NULL; host = SLIST_NEXT(host, next)) {
        assert(host->node != 0);  // Holdout from array-based known_hosts
        if (host->node != 0) {
            fprintf(out, "node add -n %d %s\n", host->node, host->name);
        }
    }

    if (out != stdout) {
        fflush(out);
        fclose(out);
    }
}


/** Public API */
void known_host_set_storage_size(uint32_t new_size){
    known_host_storage_size = new_size;
}

uint32_t known_host_get_storage_size() {
    return known_host_storage_size;
}


void host_name_completer(struct slash *slash, char * token) {
    SLIST_HEAD(known_host_s, host_s) matching_hosts = {};
    char *part_to_complete = token + strnlen(token, slash->length);
    /* Rewind to a potential whitespace */
    while(part_to_complete > token) {
        if(*part_to_complete == ' ') {
            part_to_complete++;
            break;
        }
        part_to_complete--;
    }
    size_t token_l = strnlen(part_to_complete, HOSTNAME_MAXLEN - 1);
    int cur_prefix;
    struct host_s *completion = NULL;
    int len_to_compare_to = strlen(part_to_complete);
    int matches = 0;
    int res;
    for (host_t* element = SLIST_FIRST(&known_hosts); element != NULL; element = SLIST_NEXT(element, next)) {
        res = strncmp(element->name, part_to_complete, token_l);
        if (0 == res) {
            completion = malloc(sizeof(struct host_s));
            if(completion) {
                completion->node = element->node;
                strncpy(completion->name, element->name, HOSTNAME_MAXLEN); 
                SLIST_INSERT_HEAD(&matching_hosts, completion, next);
                matches++;
            }
        }
    }
    if(matches > 1) {
        /* We only print all commands over 1 match here */
        slash_printf(slash, "\n");
    }
    struct host_s *prev_completion = NULL;
    struct host_s *cur_completion = NULL;
    int prefix_len = INT_MAX;

    SLIST_FOREACH(cur_completion, &matching_hosts, next) {
        /* Compute the length of prefix common to all completions */
        if (prev_completion != 0) {
            cur_prefix = slash_prefix_length(prev_completion->name, cur_completion->name);
            if(cur_prefix < prefix_len) {
                prefix_len = cur_prefix;
                completion = cur_completion;
            }
        }
        prev_completion = cur_completion;
        if(matches > 1) {
            slash_printf(slash, cur_completion->name);
            slash_printf(slash, "\n");
        }
    }
    if (matches == 1) {
        *part_to_complete = '\0';
        strcat(slash->buffer, completion->name);
        slash->cursor = slash->length = strlen(slash->buffer);
    } else if(matches > 1) {
        /* Fill the buffer with as much characters as possible:
        * if what the user typed in doesn't end with a space, we might
        * as well put all the common prefix in the buffer
        */
        if(slash->buffer[slash->length-1] != ' ' && len_to_compare_to < prefix_len) {
            strncpy(&slash->buffer[slash->length] - len_to_compare_to, completion->name, prefix_len);
            slash->buffer[slash->length - len_to_compare_to + prefix_len] = '\0';
            slash->cursor = slash->length = strlen(slash->buffer);
        }
    }
    /* Free up the completion list we built up earlier */
    while (!SLIST_EMPTY(&matching_hosts))
    {
        completion = SLIST_FIRST(&matching_hosts);
        SLIST_REMOVE(&matching_hosts, completion, host_s, next);
        free(completion);
    }

}

void known_hosts_del(int host) {

    // SLIST_FOREACH(host_t host, &known_hosts, next) {
    for (host_t* element = SLIST_FIRST(&known_hosts); element != NULL; element = SLIST_NEXT(element, next)) {
        if (element->node == host) {
            SLIST_REMOVE(&known_hosts, element, host_s, next);  // Probably not the best time-complexity here, O(n)*2 perhaps?
        }
    }
}

host_t * known_hosts_add(int addr, const char * new_name, bool override_existing) {

    if (addr == 0) {
        return NULL;
    }

    if (override_existing) {
        known_hosts_del(addr);  // Ensure 'addr' is not in the list
    } else {
        
        for (host_t* host = SLIST_FIRST(&known_hosts); host != NULL; host = SLIST_NEXT(host, next)) {
            if (host->node == addr) {
                return host;  // This node is already in the linked list, and we are not allowed to override it.
            }
        }
        // This node was not found in the list. Let's add it now.
    }

    // TODO Kevin: Do we want to break the API, and let the caller supply "host"?
    host_t * host = calloc(1, known_host_storage_size);
    if (host == NULL) {
        return NULL;  // No more memory
    }
    host->node = addr;
    strncpy(host->name, new_name, HOSTNAME_MAXLEN-1);  // -1 to fit NULL byte
    char address_str[64];
    snprintf(address_str, sizeof(address_str) - 1, "%d", host->node);
    csh_putvar(host->name, address_str);
    SLIST_INSERT_HEAD(&known_hosts, host, next);

    return host;
}

int known_hosts_get_name(int find_host, char * name, int buflen) {

    for (host_t* host = SLIST_FIRST(&known_hosts); host != NULL; host = SLIST_NEXT(host, next)) {
        if (host->node == find_host) {
            strncpy(name, host->name, buflen);
            return 1;
        }
    }

    return 0;

}


int known_hosts_get_node(const char * find_name) {

    if (find_name == NULL)
        return -1;

    for (host_t* host = SLIST_FIRST(&known_hosts); host != NULL; host = SLIST_NEXT(host, next)) {
        if (strncmp(find_name, host->name, HOSTNAME_MAXLEN) == 0) {
            return host->node;
        }
    }

    return -1;

}


int get_host_by_addr_or_name(void *res_ptr, const char *arg) {
    char *number_start = (char *)arg;
    char *end = NULL;
    long node = strtol(number_start, &end, 10);
    if (node != INT32_MAX && node < 16384 && *end == '\0') {  
        *(int*)res_ptr = (int)node;
        return 2;  /* Found number */
    }
    node = (long)known_hosts_get_node(arg);	
    
    /* Found node */
    if (0 <= node) {
        *(int*)res_ptr = (int)node;
        return 1;  /* Found hostname */
    }


	return 0;  /* Failed */
}
