/*
 * param_sniffer.c
 *
 *  Created on: Aug 29, 2018
 *      Author: johan
 */

#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <mpack/mpack.h>
#include <csp/csp.h>
#include <csp/csp_crc32.h>

#include "prometheus.h"

pthread_t param_sniffer_thread;

static int param_sniffer_log(void * ctx, param_queue_t *queue, param_t *param, int offset, void *reader) {

	char tmp[1000] = {};

	if (offset < 0)
		offset = 0;

	int count = 1;

	/* Inspect for array */
	mpack_tag_t tag = mpack_peek_tag(reader);
	if (tag.type == mpack_type_array) {
		count = mpack_expect_array(reader);
	}

	for (int i = offset; i < offset + count; i++) {

		struct timeval tv;
		gettimeofday(&tv, NULL);
		uint64_t time_ms = ((uint64_t) tv.tv_sec * 1000000 + tv.tv_usec) / 1000;

		switch (param->type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
			sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %u %"PRIu64"\n", param->name, param->node, i, mpack_expect_uint(reader), time_ms);
			break;
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
			sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %"PRIu64" %"PRIu64"\n", param->name, param->node, i, mpack_expect_u64(reader), time_ms);
			break;
		case PARAM_TYPE_INT8:
		case PARAM_TYPE_INT16:
		case PARAM_TYPE_INT32:
			sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %d %"PRIu64"\n", param->name, param->node, i, mpack_expect_int(reader), time_ms);
			break;
		case PARAM_TYPE_INT64:
			sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %"PRIi64" %"PRIu64"\n", param->name, param->node, i, mpack_expect_i64(reader), time_ms);
			break;
		case PARAM_TYPE_FLOAT:
			sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %f %"PRIu64"\n", param->name, param->node, i, mpack_expect_float(reader), time_ms);
			break;
		case PARAM_TYPE_DOUBLE:
			sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %f %"PRIu64"\n", param->name, param->node, i, mpack_expect_double(reader), time_ms);
			break;

		case PARAM_TYPE_STRING:
		case PARAM_TYPE_DATA:
		default:
			mpack_discard(reader);
			break;
		}

		if (mpack_reader_error(reader) != mpack_ok) {
			break;
		}

		prometheus_add(tmp);

	}


	return 0;
}

static void * param_sniffer(void * param) {
	csp_promisc_enable(100);
	while(1) {
		csp_packet_t * packet = csp_promisc_read(CSP_MAX_DELAY);

		if (packet->id.sport != PARAM_PORT_SERVER) {
			csp_buffer_free(packet);
			continue;
		}

		/* CRC32 verified packet */
		if (packet->id.flags & CSP_FCRC32) {
			if (packet->length < 4) {
				csp_log_error("Too short packet for CRC32, %u", packet->length);
				csp_buffer_free(packet);
				continue;
			}
			/* Verify CRC32 (does not include header for backwards compatability with csp1.x) */
			if (csp_crc32_verify(packet, false) != 0) {
				/* Checksum failed */
				csp_log_error("CRC32 verification error! Discarding packet");
				csp_buffer_free(packet);
				continue;
			}
		}

		uint8_t type = packet->data[0];
		if ((type != PARAM_PULL_RESPONSE) || (type != PARAM_PULL_RESPONSE_V2)) {
			csp_buffer_free(packet);
			continue;
		}

		int queue_version;
		if (type == PARAM_PULL_RESPONSE) {
		    queue_version = 1;
		} else {
		    queue_version = 2;
		}

		param_queue_t queue;
		param_queue_init(&queue, &packet->data[2], packet->length - 2, packet->length - 2, PARAM_QUEUE_TYPE_SET, queue_version);
        PARAM_QUEUE_FOREACH(param, reader, (&queue), offset)
			param_sniffer_log(NULL, &queue, param, offset, &reader);
		}
		csp_buffer_free(packet);
	}
	return NULL;
}

void param_sniffer_init(void) {
	pthread_create(&param_sniffer_thread, NULL, &param_sniffer, NULL);
}
