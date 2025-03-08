/*
 * param_list_slash.c
 *
 *  Created on: Dec 14, 2016
 *      Author: johan
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <vmem/vmem_client.h>

#include <csp/csp_crc32.h>
#include <libparam.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_string.h>

#include <endian.h>

static int list(struct slash *slash)
{
    int node = slash_dfl_node;
    int verbosity = 1;
    char * maskstr = NULL;

    optparse_t * parser = optparse_new("list", "[name wildcard=*]\n\
Will show a (filtered) list of known parameters on the specified node(s).\n\
Shows cached/known values. Use -v to include parameter type and help text.");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (-1 for all) (default = <env>)");
    optparse_add_int(parser, 'v', "verbosity", "NUM", 0, &verbosity, "verbosity (default = 1, max = 3)");
    optparse_add_string(parser, 'm', "mask", "STR", &maskstr, "mask string");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    /* Interpret maskstring */
    uint32_t mask = 0xFFFFFFFF;
    if (maskstr != NULL) {
        mask = param_maskstr_to_mask(maskstr);
    }

    char * globstr = NULL;
    if (++argi < slash->argc) {
        globstr = slash->argv[argi];
    }

    param_list_print(mask, node, globstr, verbosity);

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command(list, list, "[OPTIONS...] [name wildcard=*]", "List parameters");


static int list_download(struct slash *slash)
{
    unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int version = 3;
    int include_remotes = 0;

    optparse_t * parser = optparse_new("list download", "[node]\n\
Downloads a list of remote parameters.\n\
Fetches metadata such as name and type\n\
Metadata must be known before values can be pulled.\n\
Parameters can be manually added with 'list add'.");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 3)");
    optparse_add_set(parser, 'r', "remote", 1, &include_remotes, "Include remote params when storing list");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (++argi < slash->argc) {
        printf("Node as argument, is deprecated\n");
        node = atoi(slash->argv[argi]);
    }

    if(node == 0){
        printf("Download of local parameters not needed, use cmd 'list' instead\n");
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    param_list_download(node, timeout, version, include_remotes);

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(list, download, list_download, "[OPTIONS...] [node]", "Download a list of remote parameters");

static int list_forget(struct slash *slash)
{

    int node = slash_dfl_node;

    optparse_t * parser = optparse_new("list forget", "[node]\n\
Will remove remote parameters from the local parameter list.\n\
This makes it possible to download them again, in cases where they've changed.");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (-1 for all) (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (++argi < slash->argc) {
        printf("Node as argument, is deprecated\n");
        node = atoi(slash->argv[argi]);
    }

    printf("Removed %i parameters\n", param_list_remove(node, 1));

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(list, forget, list_forget, "[node]", "Forget remote parameters. Omit or set node to -1 to include all.");


static int list_add(struct slash *slash)
{
    unsigned int node = slash_dfl_node;
    unsigned int array_len = 0;
    int vmem_type = 0;
    char * helpstr = NULL;
    char * unitstr = NULL;
    char * maskstr = NULL;
    char * umaskstr = NULL;

    optparse_t * parser = optparse_new("list add", "<name> <id> <type>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 'a', "array", "NUM", 0, &array_len, "array length (default = none)");
    optparse_add_int(parser, 'v', "vmem", "NUM", 0, &vmem_type, "VMEM type (default = none)");
    optparse_add_string(parser, 'c', "comment", "STRING", (char **) &helpstr, "help text");
    optparse_add_string(parser, 'u', "unit", "STRING", (char **) &unitstr, "unit text");
	optparse_add_string(parser, 'm', "emask", "STRING", &maskstr, "mask (param letters)");
	optparse_add_string(parser, 'M', "umask", "STRING", &umaskstr, "user mask (param letters)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter name\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * name = slash->argv[argi];

	if (++argi >= slash->argc) {
		printf("missing parameter id\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

    char * endptr;
    unsigned int id = strtoul(slash->argv[argi], &endptr, 10);

	if (++argi >= slash->argc) {
		printf("missing parameter type\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

    char * type = slash->argv[argi];

    unsigned int typeid = param_typestr_to_typeid(type);

    if (typeid == PARAM_TYPE_INVALID) {
        printf("Invalid type %s\n", type);
        optparse_del(parser);
        return SLASH_EINVAL;
    }


	uint32_t mask = 0;

	if (maskstr)
		mask = param_maskstr_to_mask(maskstr);

	if (umaskstr)
		mask |= param_umaskstr_to_mask(umaskstr);

    //printf("name %s, id %u, type %s, typeid %u, mask %x, arraylen %u, help %s, unit %s\n", name, id, type, typeid, mask, array_len, helpstr, unitstr);

    param_t * param = param_list_create_remote(id, node, typeid, mask, array_len, name, unitstr, helpstr, vmem_type);
    if (param == NULL) {
        printf("Unable to create param\n");
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    if (param_list_add(param) != 0)
        param_list_destroy(param);

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(list, add, list_add, "<name> <id> <type>", NULL);


static int list_save_cmd(struct slash *slash) {

    char * filename = NULL;
    int node = slash_dfl_node;
    int skip_node = 0;

    optparse_t * parser = optparse_new("list save", "[name wildcard=*]");
    optparse_add_help(parser);
    optparse_add_string(parser, 'f', "filename", "PATH", &filename, "write to file");
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_set(parser, 'N', "skipnode", 1, &skip_node, "Exclude node argument");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
       return SLASH_EINVAL;
    }

    param_list_save(filename, node, skip_node);

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(list, save, list_save_cmd, "", "Save parameters");
