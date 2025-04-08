#include <stdlib.h>
#include <stdio.h>
#include <slash/slash.h>
#include <apm/csh_api.h>
#include "known_hosts.h"

static int cmd_node(struct slash *slash) {

	if (slash->argc < 1) {
		/* Only print node when explicitly asked,
			as it should be shown in the prompt.
			(it may be truncated when the hostname is too long) */
		printf("Default node = %d\n", slash_dfl_node);
	}

	if (slash->argc == 2) {

		if (0 == get_host_by_addr_or_name(&slash_dfl_node, slash->argv[1])) {
			printf("'%s' does not resolve to a valid CSP address\n", slash->argv[1]);
		}
	}

	return SLASH_SUCCESS;
}

slash_command_completer(node, cmd_node, host_name_completer, "[node]", "Set global default node");


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

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(node, save, cmd_node_save, "", "Save or print known nodes");
slash_command_sub(node, list, cmd_node_save, "", "Save or print known nodes");  // Alias

static int cmd_node_add(struct slash *slash)
{

    int node = slash_dfl_node;

    optparse_t * parser = optparse_new("node add", "<name>");
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

slash_command_sub(node, add, cmd_node_add, NULL, NULL);


static int cmd_timeout(struct slash *slash) {

	if (slash->argc == 2) {
		slash_dfl_timeout = atoi(slash->argv[1]);
	}

    printf("timeout = %d\n", slash_dfl_timeout);

	return SLASH_SUCCESS;
}
slash_command(timeout, cmd_timeout, "[timeout ms]", "Set global default timeout");