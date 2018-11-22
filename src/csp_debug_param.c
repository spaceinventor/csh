/*
 * csp_debug_param.c
 *
 *  Created on: Aug 20, 2018
 *      Author: johan
 */

#include <param/param.h>
#include <param_config.h>

extern uint8_t csp_debug_level_enabled[7];

const PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DEBUG, csp_debug,          PARAM_TYPE_UINT8,  7, sizeof(uint8_t),  PM_DEBUG, NULL, "", csp_debug_level_enabled, NULL);
