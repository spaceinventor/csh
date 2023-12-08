/*
 * stdbufmon.c
 *
 *  Created on: Jan 24, 2019
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_queue.h>
#include <param/param_client.h>
#include <vmem/vmem_server.h>
#include <csp/csp.h>
#include <sys/types.h>
#include <csp/csp_cmp.h>
#include <slash/slash.h>
#include <slash/dflopt.h>
#include <slash/optparse.h>



static vmem_list_t stdbuf_get_base(int node, int timeout) {
	vmem_list_t ret = {};

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_CRC32);
	if (conn == NULL)
		return ret;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	vmem_request_t * request = (void *) packet->data;
	request->version = 1;
	request->type = VMEM_SERVER_LIST;
	packet->length = sizeof(vmem_request_t);

	csp_send(conn, packet);

	/* Wait for response */
	packet = csp_read(conn, timeout);
	if (packet == NULL) {
		printf("No response\n");
		csp_close(conn);
		return ret;
	}

	for (vmem_list_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
		//printf(" %u: %-5.5s 0x%08X - %u typ %u\r\n", vmem->vmem_id, vmem->name, (unsigned int) be32toh(vmem->vaddr), (unsigned int) be32toh(vmem->size), vmem->type);
		if (strncmp(vmem->name, "stdbu", 5) == 0) {
			ret.vmem_id = vmem->vmem_id;
			ret.type = vmem->type;
			memcpy(ret.name, vmem->name, 5);
			ret.vaddr = be32toh(vmem->vaddr);
			ret.size = be32toh(vmem->size);
		}
	}

	csp_buffer_free(packet);
	csp_close(conn);

	return ret;
}

static int stdbuf_get(uint16_t node, uint32_t base, int from, int to, int timeout) {

	int len = to - from;
	if (len > 200)
		len = 200;

	struct csp_cmp_message message;
	message.peek.addr = htobe32(base + from);
	message.peek.len = len;

	if (csp_cmp_peek(node, timeout, &message) != CSP_ERR_NONE) {
		return 0;
	}

	int ignore __attribute__((unused)) = write(fileno(stdout), message.peek.data, len);

	return len;

}

static int stdbuf_mon_slash(struct slash *slash) {


    unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
	unsigned int version = 2;

    optparse_t * parser = optparse_new("stdbuf2", "");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "paramversion (default = 2)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Pull buffer */
	char pull_buf[25];
	param_queue_t pull_q = {
		.buffer = pull_buf,
		.buffer_size = 50,
		.type = PARAM_QUEUE_TYPE_GET,
		.used = 0,
		.version = version,
	};

	vmem_list_t vmem = stdbuf_get_base(node, 1000);

	if (vmem.size == 0 || vmem.vaddr == 0) {
		printf("Could not find stdbuffer on node %u\n", node);
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	param_t * stdbuf_in = param_list_find_id(node, 28);
	if (stdbuf_in == NULL) {
		stdbuf_in = param_list_create_remote(28, node, PARAM_TYPE_UINT16, PM_DEBUG, 0, "stdbuf_in", NULL, NULL, -1);
		param_list_add(stdbuf_in);
	}

	param_t * stdbuf_out = param_list_find_id(node, 29);
	if (stdbuf_out == NULL) {
		stdbuf_out = param_list_create_remote(29, node, PARAM_TYPE_UINT16, PM_DEBUG, 0, "stdbuf_out", NULL, NULL, -1);
		param_list_add(stdbuf_out);
	}

	param_queue_add(&pull_q, stdbuf_in, 0, NULL);
	param_queue_add(&pull_q, stdbuf_out, 0, NULL);

	printf("Monitoring stdbuf on node %u, base %x, size %u\n", node, vmem.vaddr, vmem.size);

	while(1) {

		param_pull_queue(&pull_q, CSP_PRIO_HIGH, 0, node, 100);
		int in = param_get_uint16(stdbuf_in);
		int out = param_get_uint16(stdbuf_out);

		int got = 0;
		if (out < in) {
			//printf("Get from %u to %u\n", out, in);
			got = stdbuf_get(node, vmem.vaddr, out, in, 100);
		} else if (out > in) {
			//printf("Get from %u to %u\n", out, vmem.size);
			got = stdbuf_get(node, vmem.vaddr, out, vmem.size, 100);
		}

		uint16_t out_push = out + got;
		out_push %= vmem.size;
	    param_push_single(stdbuf_out, 0, &out_push, 0, node, 100, version);

	    if (got > 0) {
	    	continue;
	    }

		/* Delay (press enter to exit) */
		if (slash_wait_interruptible(slash, 100) != 0) {
			break;
		}

	};

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(stdbuf, stdbuf_mon_slash, NULL, "Monitor stdbuf");
