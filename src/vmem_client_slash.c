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

#if 0
static int vmem_client_slash_fram(struct slash *slash, int backup) {

	int node = 0;
	int vmem_id;
	int timeout = 2000;
	char * endptr;

	if (slash->argc < 2)
		return SLASH_EUSAGE;

	vmem_id = strtoul(slash->argv[1], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	if (slash->argc >= 3) {
		node = strtoul(slash->argv[2], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	if (slash->argc >= 4) {
		timeout = strtoul(slash->argv[3], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	if (backup) {
		printf("Taking backup of vmem %u on node %u\n", vmem_id, node);
	} else {
		printf("Restoring vmem %u on node %u\n", vmem_id, node);
	}

	int result = vmem_client_backup(node, vmem_id, timeout, backup);
	if (result == -2) {
		printf("No response\n");
	} else {
		printf("Result: %d\n", result);
	}

	return SLASH_SUCCESS;
}

static int vmem_client_slash_restore(struct slash *slash)
{
	return vmem_client_slash_fram(slash, 0);
}
slash_command_sub(vmem, restore, vmem_client_slash_restore, "<vmem idx> [node] [timeout]", NULL);

static int vmem_client_slash_backup(struct slash *slash)
{
	return vmem_client_slash_fram(slash, 1);
}
slash_command_sub(vmem, backup, vmem_client_slash_backup, "<vmem idx> [node] [timeout]", NULL);

static int vmem_client_slash_unlock(struct slash *slash)
{
	int node = 0;
	int timeout = 2000;
	char * endptr;

	if (slash->argc >= 2) {
		node = strtoul(slash->argv[1], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	if (slash->argc >= 3) {
		timeout = strtoul(slash->argv[2], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	/* Step 0: Prepare request */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_NONE);
	if (conn == NULL)
		return SLASH_EINVAL;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL)
		return SLASH_EINVAL;

	vmem_request_t * request = (void *) packet->data;
	request->version = 1;
	request->type = VMEM_SERVER_UNLOCK;
	packet->length = sizeof(vmem_request_t);

	/* Step 1: Check initial unlock code */
	request->unlock.code = htobe32(0x28140360);

	csp_send(conn, packet);

	/* Step 2: Wait for verification sequence */
	if ((packet = csp_read(conn, timeout)) == NULL) {
		csp_close(conn);
		return SLASH_EINVAL;
	}

	request = (void *) packet->data;
	uint32_t sat_verification = be32toh(request->unlock.code);

	printf("Verification code received: %x\n\n", (unsigned int) sat_verification);

	printf("************************************\n");
	printf("* WARNING WARNING WARNING WARNING! *\n");
	printf("* You are about to unlock the FRAM *\n");
	printf("* Please understand the risks      *\n");
	printf("* Abort now by typing CTRL + C     *\n");
	printf("************************************\n");

	/* Step 2a: Ask user to input sequence */
	uint32_t user_verification;
	printf("Type verification sequence (you have <30 seconds): \n");

	char readbuf[9] = {};
	int cnt = 0;
	while(cnt < 8) {
		cnt += read(0, readbuf + cnt, 8-cnt);
	}
	if (sscanf(readbuf, "%x", (unsigned int *) &user_verification) != 1) {
		printf("Could not parse input\n");
		return SLASH_EINVAL;
	}

	printf("User input: %x\n", (unsigned int) user_verification);
	if (user_verification != sat_verification) {
		csp_buffer_free(packet);
		csp_close(conn);
		return SLASH_EINVAL;
	}

	/* Step 2b: Ask for final confirmation */
	printf("Validation sequence accepted\n");

	printf("Are you sure [Y/N]?\n");

	cnt = read(0, readbuf, 1);
	if (readbuf[0] != 'Y') {
		csp_buffer_free(packet);
		csp_close(conn);
		return SLASH_EINVAL;
	}

	/* Step 3: Send verification sequence */
	request->unlock.code = htobe32(user_verification);

	csp_send(conn, packet);

	/* Step 4: Check for result */
	if ((packet = csp_read(conn, timeout)) == NULL) {
		csp_close(conn);
		return SLASH_EINVAL;
	}

	request = (void *) packet->data;
	uint32_t result = be32toh(request->unlock.code);
	printf("Result: %x\n", (unsigned int) result);

	csp_close(conn);
	return SLASH_SUCCESS;

}
slash_command_sub(vmem, unlock, vmem_client_slash_unlock, "[node] [timeout]", NULL);
#endif