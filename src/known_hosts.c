/**
 * Naive, slow and simple storage of nodeid and hostname
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>

#define MAX_HOSTS 100
#define MAX_NAMELEN 50

struct host_s {
    int node;
    char name[MAX_NAMELEN];
} known_hosts[100];

void known_hosts_del(int host) {

    for (int i = 0; i < MAX_HOSTS; i++) {

        if (known_hosts[i].node == host) {
            known_hosts[i].node = 0;
        }

    }

}

void known_hosts_add(int addr, char * new_name) {

    known_hosts_del(addr);

    /* Search for empty slot */
    for (int i = 0; i < MAX_HOSTS; i++) {
        if (known_hosts[i].node == 0) {
            known_hosts[i].node = addr;
            strncpy(known_hosts[i].name, new_name, MAX_NAMELEN);
            break;
        }
    }

}

int known_hosts_get_name(int find_host, char * name, int buflen) {

    for (int i = 0; i < MAX_HOSTS; i++) {

        if (known_hosts[i].node == find_host) {
            strncpy(name, known_hosts[i].name, buflen);
            return 1;
        }

    }

    return 0;

}


int known_hosts_get_node(char * find_name) {

    if (find_name == NULL)
        return 0;

    for (int i = 0; i < MAX_HOSTS; i++) {

        if (strncmp(find_name, known_hosts[i].name, MAX_NAMELEN) == 0) {
            return known_hosts[i].node;
        }

    }

    return 0;

}


static int cmd_node_save(struct slash *slash)
{
    
    FILE * out = stdout;

    char * dirname = getenv("HOME");
    char path[100];

	if (strlen(dirname)) {
        snprintf(path, 100, "%s/csh_hosts", dirname);
    } else {
        snprintf(path, 100, "csh_hosts");
    }

    /* write to file */
	FILE * fd = fopen(path, "w");
    if (fd) {
        out = fd;
    }

    /* Search for empty slot */
    for (int i = 0; i < MAX_HOSTS; i++) {
        if (known_hosts[i].node != 0) {
            fprintf(out, "node add -n %d %s\n", known_hosts[i].node, known_hosts[i].name);
            printf("node add -n %d %s\n", known_hosts[i].node, known_hosts[i].name);
        }
    }

    if (fd) {
        fclose(fd);
    }


    return SLASH_SUCCESS;
}

slash_command_sub(node, save, cmd_node_save, NULL, NULL);


static int cmd_nodes(struct slash *slash)
{
    /* Search for empty slot */
    for (int i = 0; i < MAX_HOSTS; i++) {
        if (known_hosts[i].node != 0) {
            printf("node add -n %d %s\n", known_hosts[i].node, known_hosts[i].name);
        }
    }

    return SLASH_SUCCESS;
}

slash_command_sub(node, list, cmd_nodes, NULL, NULL);

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

	/* Check if name is present */
	if (++argi >= slash->argc) {
		printf("missing parameter name\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * name = slash->argv[argi];

    known_hosts_add(node, name);
    optparse_del(parser);
    return SLASH_SUCCESS;
}

slash_command_sub(node, add, cmd_hosts_add, NULL, NULL);
