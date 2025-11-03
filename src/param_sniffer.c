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
#include <param/param_serializer.h>
#include <mpack/mpack.h>
#include <csp/csp.h>
#include <csp/csp_hooks.h>
#include <csp/csp_crc32.h>

#include "hk_param_sniffer.h"
#include "prometheus.h"
#include "victoria_metrics.h"
#include "vts.h"

extern int prometheus_started;
extern int vm_running;

int sniffer_running = 0;
pthread_t param_sniffer_thread;
FILE *logfile;

int param_sniffer_log(void * ctx, param_queue_t *queue, param_t *param, int offset, void *reader, csp_timestamp_t *timestamp) {

    char tmp[1000] = {};

    if (offset < 0)
        offset = 0;

    int count = 1;

    /* Inspect for array */
    mpack_tag_t tag = mpack_peek_tag(reader);
    if (tag.type == mpack_type_array) {
        count = mpack_expect_array(reader);
    }

    double vts_arr[4];
    int vts = check_vts(*(param->node), param->id);

    uint64_t time_ms;
    if (timestamp->tv_sec > 0) {
        time_ms = ((uint64_t) timestamp->tv_sec * 1000000 + timestamp->tv_nsec / 1000) / 1000;
    } else {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_ms = ((uint64_t) tv.tv_sec * 1000000 + tv.tv_usec) / 1000;
    }

    for (int i = offset; i < offset + count; i++) {

        switch (param->type) {
            case PARAM_TYPE_UINT8:
            case PARAM_TYPE_XINT8:
            case PARAM_TYPE_UINT16:
            case PARAM_TYPE_XINT16:
            case PARAM_TYPE_UINT32:
            case PARAM_TYPE_XINT32:
                sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %u %"PRIu64"\n", param->name, *(param->node), i, mpack_expect_uint(reader), time_ms);
                break;
            case PARAM_TYPE_UINT64:
            case PARAM_TYPE_XINT64:
                sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %"PRIu64" %"PRIu64"\n", param->name, *(param->node), i, mpack_expect_u64(reader), time_ms);
                break;
            case PARAM_TYPE_INT8:
            case PARAM_TYPE_INT16:
            case PARAM_TYPE_INT32:
                sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %d %"PRIu64"\n", param->name, *(param->node), i, mpack_expect_int(reader), time_ms);
                break;
            case PARAM_TYPE_INT64:
                sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %"PRIi64" %"PRIu64"\n", param->name, *(param->node), i, mpack_expect_i64(reader), time_ms);
                break;
            case PARAM_TYPE_FLOAT:
                sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %e %"PRIu64"\n", param->name, *(param->node), i, mpack_expect_float(reader), time_ms);
                break;
            case PARAM_TYPE_DOUBLE: {
                double tmp_dbl = mpack_expect_double(reader);
                sprintf(tmp, "%s{node=\"%u\", idx=\"%u\"} %.12e %"PRIu64"\n", param->name, *(param->node), i, tmp_dbl, time_ms);
                if(vts){
                    vts_arr[i] = tmp_dbl;
                }
                break;
            }
                

            case PARAM_TYPE_STRING:
            case PARAM_TYPE_DATA:
            default:
                mpack_discard(reader);
                break;
        }

        if (mpack_reader_error(reader) != mpack_ok) {
            break;
        }

        if(vm_running){
            vm_add(tmp);
        }

        if(prometheus_started){
            prometheus_add(tmp);
        }

        if (logfile) {
            fprintf(logfile, "%s", tmp);
            fflush(logfile);
        }
    }

    if(vts){
        vts_add(vts_arr, param->id, count, time_ms);
    } 

    return 0;
}

int param_sniffer_crc(csp_packet_t * packet) {

    /* CRC32 verified packet */
    if (packet->id.flags & CSP_FCRC32) {
        if (packet->length < 4) {
            printf("Too short packet for CRC32, %u\n", packet->length);
            return -1;
        }
        /* Verify CRC32 (does not include header for backwards compatability with csp1.x) */
        if (csp_crc32_verify(packet) != 0) {
            /* Checksum failed */
            printf("CRC32 verification error in param sniffer! Discarding packet\n");
            return -1;
        }
    }
    return 0;
}

static void * param_sniffer(void * param) {
    csp_promisc_enable(100);
    while(1) {
        csp_packet_t * packet = csp_promisc_read(CSP_MAX_DELAY);

        if(hk_param_sniffer(packet)){
            csp_buffer_free(packet);
            continue;
        }

        if (packet->id.sport != PARAM_PORT_SERVER && packet->id.dport != PARAM_PORT_SERVER) {
            csp_buffer_free(packet);
            continue;
        }

        if (param_sniffer_crc(packet) < 0) {
            csp_buffer_free(packet);
            continue;
        }

        uint8_t type = packet->data[0];
        if ((type != PARAM_PULL_RESPONSE) && (type != PARAM_PULL_RESPONSE_V2)) {
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
        queue.last_node = packet->id.src;

        csp_timestamp_t time_now;
        csp_clock_get_time(&time_now);
        queue.last_timestamp = time_now;
        queue.client_timestamp = time_now;

        mpack_reader_t reader;
        mpack_reader_init_data(&reader, queue.buffer, queue.used);
        while(reader.data < reader.end) {
            int id, node, offset = -1;
            csp_timestamp_t timestamp = { .tv_sec = 0, .tv_nsec = 0 };
            param_deserialize_id(&reader, &id, &node, &timestamp, &offset, &queue);
            if (node == 0) {
                node = packet->id.src;
            }
            /* If parameter timestamp is not inside the header, and the lower layer found a timestamp*/
            if ((timestamp.tv_sec == 0) && (packet->timestamp_rx != 0)) {
                timestamp.tv_sec = packet->timestamp_rx;
                timestamp.tv_nsec = 0;
            }
            param_t * param = param_list_find_id(node, id);
            if (param) {
                param_sniffer_log(NULL, &queue, param, offset, &reader, &timestamp);
            } else {
                printf("Found unknown param node %d id %d\n", node, id);
                mpack_discard(&reader);
                continue;
            }
        }
        csp_buffer_free(packet);
    }
    return NULL;
}

void param_sniffer_init(int add_logfile) {

    if(sniffer_running){
        return;
    }

    if (add_logfile) {
        logfile = fopen("param_sniffer.log", "a");
        if (logfile) {
            printf("Logging parameters to param_sniffer.log\n");
        } else {
            printf("Couldn't open param_sniffer.log for append\n");
        }
    }	

    sniffer_running = 1;
    pthread_create(&param_sniffer_thread, NULL, &param_sniffer, NULL);
}
