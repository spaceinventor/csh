/*
 * tfetch.c
 *
 *  Created on: Nov 25, 2020
 *      Author: johan
 */

#include "tfetch.h"

#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <sys/types.h>
#include <csp/arch/csp_clock.h>

#include <param/param.h>

/* These come from si_param_config.h */
#define PARAMID_TFETCH_PRIMARY               41
#define PARAMID_TFETCH_SECONDARY             42
#define PARAMID_TFETCH_TIMEOUT               43
PARAM_DEFINE_STATIC_VMEM(PARAMID_TFETCH_PRIMARY,     tfetch_primary,    PARAM_TYPE_UINT16,  1, 0, PM_SYSCONF, NULL, "", tfetch, 0x00, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_TFETCH_SECONDARY,   tfetch_secondary,  PARAM_TYPE_UINT16,  1, 0, PM_SYSCONF, NULL, "", tfetch, 0x02, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_TFETCH_TIMEOUT,     tfetch_timeout,    PARAM_TYPE_UINT16,  1, 0, PM_SYSCONF, NULL, "", tfetch, 0x04, NULL);


uint16_t _tfetch_synced = 0;
uint16_t _tfetch_errors = 0;
uint32_t _tfetch_last_s = 0;
#define PARAMID_TFETCH_SYNCED                44
#define PARAMID_TFETCH_ERRORS                45
#define PARAMID_TFETCH_LAST                  46
PARAM_DEFINE_STATIC_RAM(PARAMID_TFETCH_SYNCED,       tfetch_synced,     PARAM_TYPE_UINT16,  1, 0, PM_DEBUG, NULL, "", &_tfetch_synced, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_TFETCH_ERRORS,       tfetch_errors,     PARAM_TYPE_UINT16,  1, 0, PM_DEBUG, NULL, "", &_tfetch_errors, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_TFETCH_LAST,         tfetch_last_s,     PARAM_TYPE_UINT32,  1, 0, PM_DEBUG, NULL, "", &_tfetch_last_s, NULL);


static int tfetch_remote(int node, int timeout) {

	printf("tfetch from %d timeout %d\n", node, timeout);

	/* Node 0 means disabled */
	if (node == 0) {
		return 0;
	}

	/* If timeout is not set, default to 100 */
	if (timeout == 0) {
		timeout = 100;
	}

	/* Generate empty message (GET TIME) */
	struct csp_cmp_message message;
	message.clock.tv_sec = 0;
	message.clock.tv_nsec = 0;

	if (csp_cmp_clock(node, timeout, &message) != CSP_ERR_NONE) {
		printf("No response from %d within %d ms\n", node, timeout);
		param_set_uint16(&tfetch_errors, param_get_uint16(&tfetch_errors) + 1);
		return 0;
	}

	/* Take care of endianness */
	csp_timestamp_t clock;
	clock.tv_sec = be32toh(message.clock.tv_sec);
	clock.tv_nsec = be32toh(message.clock.tv_nsec);

	/* Time needs to be after 1. Jan 2020 00:00:00 */
	if (clock.tv_sec < 1577836800) {
		printf("Invalid timestamp received from %d\n", node);
		param_set_uint16(&tfetch_errors, param_get_uint16(&tfetch_errors) + 1);
		return 0;
	}

	/* Apply clock to system */
	if (csp_clock_set_time(&clock) == CSP_ERR_NONE) {
		printf("Got time from %d\n", node);
		return node;
	}

	/* Remember last timesync */
	_tfetch_last_s = clock.tv_sec;

	param_set_uint16(&tfetch_errors, param_get_uint16(&tfetch_errors) + 1);
	return 0;

}

/* Run this function periodically to run timesync */
void tfetch_onehz(void) {

	/* Try primary */
	if (!_tfetch_synced) {
		_tfetch_synced = tfetch_remote(param_get_uint16(&tfetch_primary), param_get_uint16(&tfetch_timeout));
	}

	/* Try secondary */
	if (!_tfetch_synced) {
		_tfetch_synced = tfetch_remote(param_get_uint16(&tfetch_secondary), param_get_uint16(&tfetch_timeout));
	}

}
