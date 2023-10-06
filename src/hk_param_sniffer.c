/*
 * hk_param_sniffer.c
 *
 *  Created on: Mar 2, 2022
 *      Author: Troels
 */

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <mpack/mpack.h>
#include <csp/csp.h>
#include <csp/csp_crc32.h>

#include <slash/slash.h>
#include <slash/dflopt.h>
#include <slash/optparse.h>

#include "prometheus.h"
#include "param_sniffer.h"

pthread_t hk_param_sniffer_thread;

static time_t local_epoch = 0;

void hk_epoch(time_t epoch) {

	local_epoch = epoch;
	printf("Setting satellite EPOCH to %s", ctime(&local_epoch));
}

static int hk_timeoffset(struct slash *slash) {

    optparse_t * parser = optparse_new("hk timeoffset", "Satellite epoch time in seconds relative to Jan 1th 1970");
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Check if name is present */
	int time_offset = 0;
	if (++argi < slash->argc) {
		time_offset = atoi(slash->argv[argi]);
	}

	
	if (time_offset > 0) {
		hk_epoch(time_offset);
	} else {
		printf("Current satellite EPOCH is %s", ctime(&local_epoch));
	}

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command_sub(hk, timeoffset, hk_timeoffset, NULL, NULL);

void hk_param_sniffer(csp_packet_t * packet) {

	if (packet->id.sport != 13) {
		return;
	}

	if (param_sniffer_crc(packet) < 0) {
		return;
	}

	/* Protocol has a header size of 5, and RDP adds 5 bytes to the end of the packet if activated */
	size_t header_size = 5;
	size_t data_len = packet->length - header_size - ((packet->id.flags & CSP_FRDP) ? 5 : 0);
	param_queue_t queue;
	param_queue_init(&queue, &packet->data[header_size], data_len, data_len, PARAM_QUEUE_TYPE_SET, 2);
	queue.last_node = packet->id.src;

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, queue.buffer, queue.used);
	while(reader.data < reader.end) {
		int id, node, offset = -1;
		long unsigned int timestamp = 0;
		param_deserialize_id(&reader, &id, &node, &timestamp, &offset, &queue);
		if (node == 0) {
			node = packet->id.src;
		}
		param_t * param = param_list_find_id(node, id);
		if (param) {
			*param->timestamp = timestamp;
			if (*param->timestamp == 0 || local_epoch == 0) {
				printf("EPOCH or param timestamp is missing for %u:%s, logging is aborted %u %lu\n", param->node, param->name, *param->timestamp, local_epoch);
				break;
			}
			if (*param->timestamp != 0) {
				*param->timestamp += local_epoch;
			}
			param_sniffer_log(NULL, &queue, param, offset, &reader, *param->timestamp, packet->id.src);
		}
	}
}
