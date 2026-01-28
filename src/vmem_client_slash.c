/*
 * vmem_client_slash.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */


#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <vmem/vmem_client.h>
#include <vmem/vmem_server.h>
#include <vmem/vmem_codec.h>
#include <csp/csp.h>
#include <sys/types.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <apm/csh_api.h>

static int vmem_client_slash_list(struct slash *slash)
{
	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int version = 2;

    optparse_t * parser = optparse_new("vmem", NULL);
    optparse_add_help(parser);
    csh_add_node_option(parser, &node);
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 2)");
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	printf("Requesting vmem list from node %u timeout %u version %d\n", node, timeout, version);

	vmem_client_list(node, timeout, version);
    optparse_del(parser);
	return SLASH_SUCCESS;

}
slash_command(vmem, vmem_client_slash_list, "", "List virtual memory");

static int vmem_client_slash_decompress(struct slash *slash) {

	int res = SLASH_SUCCESS;

	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;
	unsigned int version = 1;

	uint64_t src_address;
	uint64_t dst_address;
	uint32_t length;

	optparse_t * parser = optparse_new("vmem decompress", "<src_address> <dst_address> [length base10 or base16]");
	optparse_add_help(parser);
	csh_add_node_option(parser, &node);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
	optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 1)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
	if (argi < 0) { 
		res = SLASH_EINVAL;
		goto failure;
	}

	res = parse_vmem_address(slash, &argi, &src_address);
	if (res < 0) { goto failure; }

	res = parse_vmem_address(slash, &argi, &dst_address);
	if (res < 0) { goto failure; }

	res = parse_length(slash, &argi, &length);
	if (res < 0) { goto failure; }

	res = vmem_client_decompress(node, timeout, src_address, dst_address, length, version);

	if (res < 0) {
		if (res == -1) {
			res = SLASH_ENOMEM;
		} else if (res == -2) {
			printf("\033[31m\n");
			printf("Timed out on codec response\n");
			printf("\033[0m\n");
			res = SLASH_EIO;
		}
	}

failure:
	optparse_del(parser);
	return res;
}
slash_command_sub(vmem, decompress, vmem_client_slash_decompress, "<src_address> <dst_address> <length>", "Perform decompression from src into dst");

static int vmem_client_slash_compress(struct slash *slash) {

	int res = SLASH_SUCCESS;

	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;
	unsigned int version = 1;

	uint64_t src_address;
	uint64_t dst_address;
	uint32_t length;

	optparse_t * parser = optparse_new("vmem compress", "<src_address> <dst_address> [length base10 or base16]");
	optparse_add_help(parser);
	csh_add_node_option(parser, &node);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
	optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 1)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
	if (argi < 0) { 
		res = SLASH_EINVAL;
		goto failure;
	}

	res = parse_vmem_address(slash, &argi, &src_address);
	if (res < 0) { goto failure; }

	res = parse_vmem_address(slash, &argi, &dst_address);
	if (res < 0) { goto failure; }

	res = parse_length(slash, &argi, &length);
	if (res < 0) { goto failure; }

	res = vmem_client_compress(node, timeout, src_address, dst_address, length, version);

	if (res < 0) {
		if (res == -1) {
			res = SLASH_ENOMEM;
		} else if (res == -2) {
			printf("\033[31m\n");
			printf("Timed out on codec response\n");
			printf("\033[0m\n");
			res = SLASH_EIO;
		}
	}

failure:
	optparse_del(parser);
	return res;
}
slash_command_sub(vmem, compress, vmem_client_slash_compress, "<src_address> <dst_address> <length>", "Perform compression from src into dst");

