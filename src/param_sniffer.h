/*
 * param_sniffer.h
 *
 *  Created on: Aug 29, 2018
 *      Author: johan
 */

#ifndef SRC_PARAM_SNIFFER_H_
#define SRC_PARAM_SNIFFER_H_

#include <csp/csp.h>
#include <param/param.h>
#include <param/param_queue.h>

int param_sniffer_crc(csp_packet_t * packet);
int param_sniffer_log(void * ctx, param_queue_t *queue, param_t *param, int offset, void *reader, csp_timestamp_t *timestamp);
void param_sniffer_init(int add_logfile);

/* TODO Kevin: We should probably pass along `*queue`, `offset` and `*timestamp` as well. */
typedef void (*param_sniffer_callback_f)(param_t * param, void * context);
/**
 * @brief Register a function to be called when a parameter is sniffed and deserialized from the network.
 * 
 * @param callback[in]		Callback function to call whenever a decoding error is encountered.
 * @param context[in] 		User-supplied context for the callback function. Must remain valid for as long as the function is registered (is not copied).
 * @return int 					0 OK, <0 ERROR
 */
int param_sniffer_register_callback(param_sniffer_callback_f callback, void * context);
int param_sniffer_unregister_callback(param_sniffer_callback_f callback);

#endif /* SRC_PARAM_SNIFFER_H_ */
