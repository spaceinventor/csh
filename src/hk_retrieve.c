/*
 * hk_retrieve.c
 *
 *  Created on: Mar 2, 2022
 *      Author: Troels
 */

#include <stdio.h>
#include <endian.h>
#include <sys/time.h>
#include <time.h>

#include <slash/slash.h>
#include <slash/dflopt.h>
#include <slash/optparse.h>

#include <param/param_queue.h>

#include <csp/csp.h>
#include <csp/csp_cmp.h>

#include "hk_param_sniffer.h"

void param_deserialize_id(mpack_reader_t *reader, int *id, int *node, long unsigned int *timestamp, int *offset, param_queue_t * queue);
void param_deserialize_from_mpack_to_param(void * context, void * queue, param_t * param, int offset, mpack_reader_t * reader);
void param_print_file(FILE* file, param_t * param, int offset, int nodes[], int nodes_count, int verbose);

static int hk_retrieve(struct slash *slash) {

	unsigned int node = slash_dfl_node;
	char * filename = NULL;
    uint32_t timestamp = 0;
    uint32_t step = 1;
    uint32_t num_timestamps = 1;
    char * prio = "123";
    int rdp = 0;
    int use_offset = 0;

    optparse_t * parser = optparse_new("hk retrieve", "timestamp");
    optparse_add_help(parser);
    optparse_add_string(parser, 'f', "file", "FILENAME", &filename, "File to store downloaded telemetry");
    optparse_add_unsigned(parser, 't', "time", "NUM", 0, &timestamp, "Timestamp for newest telemetry to request");
    optparse_add_unsigned(parser, 's', "step", "NUM", 0, &step, "Step between telemetry timestamps");
    optparse_add_unsigned(parser, 'N', "num", "NUM", 0, &num_timestamps, " Number of timestamps to download");
    optparse_add_set(parser, 'o', "offset", 1, &use_offset, "Calculate time offset from timestamp in reply");
    optparse_add_string(parser, 'p', "prio", "STRING", &prio, "Log priorities (default 123)");
    optparse_add_set(parser, 'r', "rdp", CSP_O_RDP, &rdp, "Use RDP protocol for downloads");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (use_offset && timestamp != 0) {
        printf("Options offset and timestamp cannot be used simultaneously\n");
	    return SLASH_EINVAL;
    }

    csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, node, 13, slash_dfl_timeout, CSP_O_CRC32 | rdp );
    if (conn == NULL) {
        printf("Could not create connection");
        return SLASH_EIO;
    }

    csp_packet_t * packet = csp_buffer_get(8);
	if (packet == NULL) {
        printf("Could not create packet");
		csp_close(conn);
		return SLASH_EIO;
	}

    uint8_t prio_mask = 0;
    if (strchr(prio, '1') != NULL) prio_mask |= 0b001;
    if (strchr(prio, '2') != NULL) prio_mask |= 0b010;
    if (strchr(prio, '3') != NULL) prio_mask |= 0b100;

    packet->data32[0] = htobe32(timestamp);
    packet->data16[2] = htobe16(step);
    packet->data[6] = prio_mask;
    packet->data[7] = (uint8_t)(num_timestamps&0xFF);
    packet->length = 8;

	csp_send(conn, packet);

    long unsigned int last_timestamp = 0;

    FILE * file = stdout;
    if (filename != NULL) {
        file = fopen(filename, "a");
    }
    if (file == NULL) {
        printf("File could not be opened, redirecting to stdout\n");
        file = stdout;
    }

    do {
        csp_buffer_free(packet);
        packet = csp_read(conn, slash_dfl_timeout);
        if (packet == NULL) {
            printf("No response\n");
            csp_close(conn);
            return SLASH_EIO;
        }

        if (last_timestamp != be32toh(packet->data32[0])) {
            last_timestamp = be32toh(packet->data32[0]);
            fprintf(file, "Timestamp %lu\n", last_timestamp);
        }

        if (use_offset) {
			struct timeval tv;
			gettimeofday(&tv, NULL);
            hk_epoch(tv.tv_sec - last_timestamp);
            use_offset = 0;
        }

        param_queue_t queue;
        param_queue_init(&queue, &packet->data[5], packet->length - 5, packet->length - 5, PARAM_QUEUE_TYPE_GET, 2);

		mpack_reader_t reader;
		mpack_reader_init_data(&reader, queue.buffer, queue.used);
		while(reader.data < reader.end) {
			int id, node_param, offset = -1;
			long unsigned int timestamp_param = 0;
			param_deserialize_id(&reader, &id, &node_param, &timestamp_param, &offset, &queue);
			param_t * param = param_list_find_id(node_param, id);
            if (param == NULL) {
                printf("Parameter with node %d and id %d not found\n", node_param, id);
                break;
            }
            param_t param_log = *param;
            *param_log.timestamp = timestamp_param;

			param_deserialize_from_mpack_to_param(NULL, &queue, &param_log, offset, &reader);

            /* Handle received parameter data here */
            param_print_file(file, &param_log, -1, NULL, 0, 1);
    	}

    } while ((packet->data[4] & (1 << 7)) == 0);
    csp_buffer_free(packet);

    fprintf(file, "\n");
    if (file != stdout) {
        fflush(file);
        fclose(file);
    }
    
	csp_close(conn);
    return SLASH_SUCCESS;
}

slash_command_sub(hk, retrieve, hk_retrieve, NULL, NULL);
