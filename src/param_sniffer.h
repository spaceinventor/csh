/*
 * param_sniffer.h
 *
 *  Created on: Aug 29, 2018
 *      Author: johan
 */

#ifndef SRC_PARAM_SNIFFER_H_
#define SRC_PARAM_SNIFFER_H_

#include <csp/csp.h>
#include <param/param_queue.h>

int param_sniffer_crc(csp_packet_t * packet);
int param_sniffer_log(void * ctx, param_queue_t *queue, param_t *param, int offset, void *reader, csp_timestamp_t *timestamp);
void param_sniffer_init(int add_logfile);

#endif /* SRC_PARAM_SNIFFER_H_ */
