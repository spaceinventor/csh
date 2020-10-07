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
#include <csp/csp_endian.h>
#include <csp/csp_cmp.h>
#include <slash/slash.h>



static vmem_list_t stdbuf_get_base(int node, int timeout) {
	vmem_list_t ret = {};

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_NONE);
	if (conn == NULL)
		return ret;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	vmem_request_t * request = (void *) packet->data;
	request->version = VMEM_VERSION;
	request->type = VMEM_SERVER_LIST;
	packet->length = sizeof(vmem_request_t);

	if (!csp_send(conn, packet, VMEM_SERVER_TIMEOUT)) {
		csp_buffer_free(packet);
		return ret;
	}

	/* Wait for response */
	packet = csp_read(conn, timeout);
	if (packet == NULL) {
		printf("No response\n");
		csp_close(conn);
		return ret;
	}

	for (vmem_list_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
		//printf(" %u: %-5.5s 0x%08X - %u typ %u\r\n", vmem->vmem_id, vmem->name, (unsigned int) csp_ntoh32(vmem->vaddr), (unsigned int) csp_ntoh32(vmem->size), vmem->type);
		if (strncmp(vmem->name, "stdbu", 5) == 0) {
			ret.vmem_id = vmem->vmem_id;
			ret.type = vmem->type;
			memcpy(ret.name, vmem->name, 5);
			ret.vaddr = csp_ntoh32(vmem->vaddr);
			ret.size = csp_ntoh32(vmem->size);
		}
	}

	csp_buffer_free(packet);
	csp_close(conn);

	return ret;
}

static int stdbuf_get(uint8_t node, uint32_t base, int from, int to, int timeout) {

	int len = to - from;
	if (len > 200)
		len = 200;

	struct csp_cmp_message message;
	message.peek.addr = csp_hton32(base + from);
	message.peek.len = len;

	if (csp_cmp_peek(node, timeout, &message) != CSP_ERR_NONE) {
		return 0;
	}

	int ignore __attribute__((unused)) = write(fileno(stdout), message.peek.data, len);

	return len;

}

static int stdbuf_mon_slash(struct slash *slash) {

	if (slash->argc != 2)
		return SLASH_EUSAGE;

	uint8_t node = atoi(slash->argv[1]);

	/* Pull buffer */
	char pull_buf[25];
	param_queue_t pull_q = {
		.buffer = pull_buf,
		.buffer_size = 25,
		.type = PARAM_QUEUE_TYPE_GET,
		.used = 0,
	};

	param_t * stdbuf_in = param_list_find_id(node, 28);
	param_t * stdbuf_out = param_list_find_id(node, 29);

	if (!stdbuf_in || !stdbuf_out) {
		printf("Please download stdbuf_in and _out parameters first\n");
		return SLASH_EINVAL;
	}

	param_queue_add(&pull_q, stdbuf_in, 0, NULL);
	param_queue_add(&pull_q, stdbuf_out, 0, NULL);

	vmem_list_t vmem = stdbuf_get_base(node, 1000);

	if (vmem.size == 0 || vmem.vaddr == 0) {
		printf("Could not find stdbuffer on node %u\n", node);
		return SLASH_EINVAL;
	}

	printf("Monitoring stdbuf on node %u, base %x, size %u\n", node, vmem.vaddr, vmem.size);

	while(1) {

		param_pull_queue(&pull_q, 0, node, 1000);
		int in = param_get_uint16(stdbuf_in);
		int out = param_get_uint16(stdbuf_out);

		int got = 0;
		if (out < in) {
			//printf("Get from %u to %u\n", out, in);
			got = stdbuf_get(node, vmem.vaddr, out, in, 1000);
		} else if (out > in) {
			//printf("Get from %u to %u\n", out, vmem.size);
			got = stdbuf_get(node, vmem.vaddr, out, vmem.size, 1000);
		}

		uint16_t out_push = out + got;
		out_push %= vmem.size;
	    param_push_single(stdbuf_out, 0, &out_push, 0, node, 1000, 2);

	    if (got > 0) {
	    	continue;
	    }

		/* Delay (press enter to exit) */
		if (slash_wait_interruptible(slash, 1000) != 0) {
			break;
		}

	};
	return SLASH_SUCCESS;
}

slash_command(stdbuf, stdbuf_mon_slash, NULL, "Monitor stdbuf");
