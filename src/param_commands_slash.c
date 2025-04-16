/*
 * param_commands_slash.c
 *
 *  Created on: Sep 22, 2021
 *      Author: Mads
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <apm/csh_api.h>

#include <csp/csp.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include "param_commands_client.h"

extern param_queue_t param_queue;

static int cmd_server_upload(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;

    optparse_t * parser = optparse_new("cmd server upload", "<name>");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_custom(parser, 's', "server", "NUM", "server to push parameters to (default = <env>))", get_host_by_addr_or_name, &server);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }


	if (++argi >= slash->argc) {
		printf("Must specify <name>\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char *name = slash->argv[argi];

	if (param_command_push(&param_queue, 1, server, name, timeout) < 0) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EIO;
	}

    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_subsub(cmd, server, upload, cmd_server_upload, "", NULL);

static int cmd_server_download(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;

    optparse_t * parser = optparse_new("cmd server download", "<name>");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_custom(parser, 's', "server", "NUM", "server to push parameters to (default = <env>))", get_host_by_addr_or_name, &server);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }


	if (++argi >= slash->argc) {
		printf("Must specify <name>\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char *name = slash->argv[argi];

	if (param_command_download(server, 1, name, timeout) < 0) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EIO;
	}

    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_subsub(cmd, server, download, cmd_server_download, "<name>", NULL);

static int cmd_server_list(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;

    optparse_t * parser = optparse_new("cmd server list", NULL);
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_custom(parser, 's', "server", "NUM", "server to push parameters to (default = <env>))", get_host_by_addr_or_name, &server);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (param_command_list(server, 1, timeout) < 0) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EIO;
	}

    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_subsub(cmd, server, list, cmd_server_list, "", NULL);

static int cmd_server_rm(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;
	int rm_all = 0;

    optparse_t * parser = optparse_new("cmd server rm", "<name>");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_set(parser, 'a', "all", 1, &rm_all, "delete all");
	optparse_add_custom(parser, 's', "server", "NUM", "server to push parameters to (default = <env>))", get_host_by_addr_or_name, &server);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }


	if (++argi >= slash->argc) {
		printf("Must specify <name>\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char *name = slash->argv[argi];

	if (rm_all == 1) {
		if (param_command_rm_all(server, 1, name, timeout) < 0) {
			printf("No response\n");
            optparse_del(parser);
			return SLASH_EIO;
		}
	} else if (rm_all == 0) {
		if (param_command_rm(server, 1, name, timeout) < 0) {
			printf("No response\n");
            optparse_del(parser);
			return SLASH_EIO;
		}
	}

    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_subsub(cmd, server, rm, cmd_server_rm, "<name>", NULL);