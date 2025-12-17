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
#include <param/param_serializer.h>
#include <mpack/mpack.h>
#include <csp/csp.h>
#include <csp/csp_crc32.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <apm/csh_api.h>

#include "param_sniffer.h"

pthread_t hk_param_sniffer_thread;
#define MAX_HKS 16

typedef struct local_epoch_s {
	int count;
	time_t local_epoch[MAX_HKS];
	uint16_t node[MAX_HKS];
} local_epoch_t;
static local_epoch_t hks = {0};

typedef struct timesync_nodes_s {
	int count;
	uint16_t node[MAX_HKS];
	uint16_t paramid[MAX_HKS];
} timesync_nodes_t;
static timesync_nodes_t timesync_nodes = {0};

static void hk_set_utcparam(unsigned int node, unsigned int paramid) {

	// update existing
	for (int i = 0; i < timesync_nodes.count; i++) {
		if (timesync_nodes.node[i] == node) {
			timesync_nodes.node[i] = node;
			timesync_nodes.paramid[i] = paramid;
			printf("Updating HK UTC parameter from node %u\n", node);
			return;
		}
	}

	if (timesync_nodes.count >= MAX_HKS) {
		printf("Error: Maximum number of HK nodes reached (%d). Cannot set new utcparam for node %u\n", MAX_HKS, node);
		return;
	}

	timesync_nodes.node[timesync_nodes.count] = node;
	timesync_nodes.paramid[timesync_nodes.count++] = paramid;

	printf("Adding HK UTC parameter from node %u\n", node);
}

static int hk_utcparam(struct slash * slash) {

	optparse_t * parser = optparse_new_ex("hk utcparam", "[node:]param_name", "Reference to parameter containing UNIX time for automatic timesync. Type supported: uint32");
	optparse_add_help(parser);
	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);

	if (slash->argc < 2) {
		printf("Param is missing\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	argi++;

	char * semicolon = strchr(slash->argv[argi], ':');

	unsigned int node = slash_dfl_node;
	if (semicolon > slash->argv[argi]) {
		*semicolon = '\0';
		if (0 >= get_host_by_addr_or_name(&node, slash->argv[argi])) {
			fprintf(stderr, "'%s' does not resolve to a valid CSP address\n", slash->argv[argi]);
			optparse_del(parser);
			return SLASH_EINVAL;
		}
	} else {
		/* Node is not included, so we fake a semicolon before the string */
		semicolon = slash->argv[argi] - 1;
	}
	param_t * utcparam = param_list_find_name(node, semicolon + 1);

	if (utcparam == NULL) {
		printf("Parameter is not known by CSH\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	hk_set_utcparam(node, utcparam->id);

	return SLASH_SUCCESS;
}
slash_command_sub(hk, utcparam, hk_utcparam, NULL, NULL);

void hk_set_epoch(time_t epoch, uint16_t node, bool auto_sync) {

	time_t current_epoch;
	time(&current_epoch);
	char current_time_str[32];
	strftime(current_time_str, sizeof(current_time_str), "%Y-%m-%d %H:%M:%S", gmtime(&current_epoch));

	/* 1577833200: Jan 1st 2020 */
	if (epoch > current_epoch || epoch < 1577833200) {
		char current_epoch_str[32];
		strftime(current_epoch_str, sizeof(current_epoch_str), "%Y-%m-%d %H:%M:%S", gmtime(&epoch));
		printf("At %s: Illegal EPOCH %lu (%s) received\n", current_time_str, current_epoch, current_epoch_str);
		return;
	}

	/* update existing */
	for (int i = 0; i < hks.count; i++) {
		if (hks.node[i] == node) {

			if (auto_sync && hks.local_epoch[i] - epoch > 86400) {
				char time[32];
				strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", gmtime(&epoch));
				char time_current[32];
				strftime(time_current, sizeof(time_current), "%Y-%m-%d %H:%M:%S", gmtime(&hks.local_epoch[i]));
				printf("At %s: Skipping possible invalid EPOCH %s, current EPOCH for HK node %u is %s\n", current_time_str, time, node, time_current);
				return;
			}

			if (labs(hks.local_epoch[i] - epoch) > 1 || !auto_sync) {
				/* get unix time to string time */
				char time[32];
				strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", gmtime(&epoch));
				printf("At %s: Updating HK node %u EPOCH by %ld sec to %s\n", current_time_str, node, hks.local_epoch[i] - epoch, time);
			}

			hks.local_epoch[i] = epoch;
			return;
		}
	}

	if (hks.count >= MAX_HKS) {
		printf("At %s: Error: Maximum number of HK nodes reached (%d). Cannot set new epoch for node %u\n", current_time_str, MAX_HKS, node);
		return;
	}

	hks.node[hks.count] = node;
	hks.local_epoch[hks.count++] = epoch;

	printf("At %s: Setting new hk node %u EPOCH to %ld\n", current_time_str, node, epoch);
}

static int hk_timeoffset(struct slash * slash) {

	unsigned int node = slash_dfl_node;
	optparse_t * parser = optparse_new_ex("hk timeoffset", "[epoch]", "Satellite epoch time in seconds relative to Jan 1th 1970");
	csh_add_node_option(parser, &node);
	optparse_add_help(parser);
	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Check if time_offset is present */
	int time_offset = 0;
	if (++argi < slash->argc) {
		time_offset = atoi(slash->argv[argi]);
	}

	if (time_offset > 0) {
		hk_set_epoch(time_offset, node, false);
	} else {
		for (int i = 0; i < hks.count; i++) {
			if (hks.node[i] == node) {
				printf("Current satellite EPOCH is %s\nSeconds: %lu\n", ctime(&hks.local_epoch[i]), hks.local_epoch[i]);
			}
		}
	}

	optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command_sub(hk, timeoffset, hk_timeoffset, NULL, NULL);

bool hk_get_epoch(time_t * local_epoch, uint16_t node) {

	for (int i = 0; i < hks.count; i++) {
		if (node == hks.node[i]) {
			*local_epoch = hks.local_epoch[i];
			return true;
		}
	}

	return false;
}

bool hk_param_sniffer(csp_packet_t * packet) {

	if (packet->id.sport != 13) {
		return false;
	}

	if (param_sniffer_crc(packet) < 0) {
		return false;
	}

	/* Protocol has a header size of 5, and RDP adds 5 bytes to the end of the packet if activated */
	size_t header_size = 5;
	size_t data_len = packet->length - header_size - ((packet->id.flags & CSP_FRDP) ? 5 : 0);
	param_queue_t queue;
	param_queue_init(&queue, &packet->data[header_size], data_len, data_len, PARAM_QUEUE_TYPE_SET, 2);
	queue.last_node = packet->id.src;

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, queue.buffer, queue.used);
	static bool epoch_notfound_warning = false; // Only print this warning once
	while (reader.data < reader.end) {
		int id, node, offset = -1;
		csp_timestamp_t timestamp = { .tv_sec = 0, .tv_nsec = 0 };
		param_deserialize_id(&reader, &id, &node, &timestamp, &offset, &queue);
		if (node == 0) {
			node = packet->id.src;
		}
		param_t * param = param_list_find_id(node, id);
		if (param) {
			*param->timestamp = timestamp;
			if (param->timestamp->tv_sec == 0) {
				printf("HK: Param timestamp is missing for %u:%s, logging is aborted\n", *(param->node), param->name);
				break;
			}

			time_t local_epoch = -1;
			for (int i = 0; i < timesync_nodes.count; i++) {
				if (timesync_nodes.node[i] == node && timesync_nodes.paramid[i] == param->id) {
					mpack_tag_t tag = mpack_peek_tag(&reader);
					local_epoch = tag.v.i - timestamp.tv_sec;
					hk_set_epoch(local_epoch, packet->id.src, true);
					break;
				}
			}
			if (local_epoch == -1 && !hk_get_epoch(&local_epoch, packet->id.src)) {
				if(!epoch_notfound_warning) {
					printf("HK: No local epoch found for node %u, skipping\n", packet->id.src);
					epoch_notfound_warning = true;
				}
				mpack_discard(&reader);
				continue;
			}

			param->timestamp->tv_sec += local_epoch;
			param_sniffer_log(NULL, &queue, param, offset, &reader, param->timestamp);
		} else {
			printf("HK: Found unknown param node %d id %d\n", node, id);
			mpack_discard(&reader);
			continue;
		}
	}
	return true;
}
