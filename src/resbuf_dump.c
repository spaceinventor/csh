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
#include <vmem/vmem_client.h>
#include <csp/csp.h>
#include <sys/types.h>
#include <csp/csp_cmp.h>
#include <slash/slash.h>
#include <slash/dflopt.h>
#include <slash/optparse.h>
#include <time.h>



static vmem_list_t resbuf_get_base(int node, int timeout) {
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
		if (strncmp(vmem->name, "resbu", 5) == 0) {
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

static int resbuf_dump_slash(struct slash *slash) {

    unsigned int node = slash_dfl_node;
	char * filename = NULL;

    optparse_t * parser = optparse_new("resbuf", "");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_string(parser, 'f', "filename", "PATH", &filename, "write to file, or 'timestamp' for timestamped file in cwd");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	FILE * fpout = stdout;

	if(filename) {
		if (strcmp(filename, "timestamp") == 0) {
			time_t t = time(NULL);
			struct tm tm = *localtime(&t);
			char timestamp[16];
			strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm);
			char filename2[32];
			snprintf(filename2, sizeof(filename2), "%u_%s.txt", node, timestamp);
			filename = filename2;
		}

		FILE *fp = fopen(filename, "w");
		if (fp) {
			fpout = fp;
			printf("Writing to file %s\n", filename);
		}
	}

	vmem_list_t vmem = resbuf_get_base(node, 1000);
	if (vmem.size == 0 || vmem.vaddr == 0) {
		printf("Could not find result buffer on node %u\n", node);
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char data[vmem.size];

	vmem_download(node, 1000, vmem.vaddr, vmem.size, data, 2, 1);

	//csp_hex_dump("data", data, 200);

	uint16_t in;
	memcpy(&in, &data[0], sizeof(in));
	uint16_t out;
	memcpy(&out, &data[2], sizeof(out));

	printf("Got resbuf size %u in %u out %u\n", vmem.size, in, out);

	if (in > vmem.size)
        optparse_del(parser);
		return SLASH_EINVAL;

	if (out > vmem.size)
        optparse_del(parser);
		return SLASH_EINVAL;


	while(1) {
		fprintf(fpout, "%c", data[out++]);
		if (out >= vmem.size) {
			out = 4;
		}
		if (out == in) {
			break;
		}
	}

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(resbuf, resbuf_dump_slash, NULL, "Monitor stdbuf");
