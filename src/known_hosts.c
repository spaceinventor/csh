/**
 * Naive, slow and simple storage of nodeid and hostname
 */

#include "known_hosts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>

/*Both of these may be modified by APMs  */
__attribute__((used, retain)) unsigned int known_host_storage_size = sizeof(host_t);
SLIST_HEAD(known_host_s, host_s) known_hosts = {};

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
        return 0;

    for (host_t* host = SLIST_FIRST(&known_hosts); host != NULL; host = SLIST_NEXT(host, next)) {
        if (strncmp(find_name, host->name, HOSTNAME_MAXLEN) == 0) {
            return host->node;
        }
    }

    return 0;

}

static void node_save(const char * filename) {

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

const struct slash_command slash_cmd_node_save;
static int cmd_node_save(struct slash *slash) {
    char * filename = NULL;

    optparse_t * parser = optparse_new_ex(slash_cmd_node_save.name, slash_cmd_node_save.args, slash_cmd_node_save.help);
    optparse_add_help(parser);
    optparse_add_string(parser, 'f', "filename", "PATH", &filename, "write to file");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    node_save(filename);

    return SLASH_SUCCESS;
}
slash_command_sub(node, save, cmd_node_save, "", "Save or print known nodes");
slash_command_sub(node, list, cmd_node_save, "", "Save or print known nodes");  // Alias

static int cmd_hosts_add(struct slash *slash)
{

    int node = slash_dfl_node;

    optparse_t * parser = optparse_new("hosts add", "<name>");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (node == 0) {
        fprintf(stderr, "Refusing to add hostname for node 0");
        optparse_del(parser);
        return SLASH_EINVAL;
    }

	/* Check if name is present */
	if (++argi >= slash->argc) {
		printf("missing node hostname\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * name = slash->argv[argi];

    if (known_hosts_add(node, name, true) == NULL) {
        fprintf(stderr, "No more memory, failed to add host");
        optparse_del(parser);
        return SLASH_ENOMEM;  // We have already checked for node 0, so this can currently only be a memory error.
    }
    optparse_del(parser);
    return SLASH_SUCCESS;
}

slash_command_sub(node, add, cmd_hosts_add, NULL, NULL);
