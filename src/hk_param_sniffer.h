/*
 * hk_param_sniffer.h
 *
 *  Created on: Mar 2, 2022
 *      Author: Troels
 */

#ifndef SRC_HK_PARAM_SNIFFER_H_
#define SRC_HK_PARAM_SNIFFER_H_

#include <time.h>
#include <csp/csp.h>

/* returns true if the packet was found to be for housekeeping */
bool hk_param_sniffer(csp_packet_t * packet);

#endif /* SRC_HK_PARAM_SNIFFER_H_ */
