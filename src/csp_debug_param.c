/*
 * csp_debug_param.c
 *
 *  Created on: Aug 20, 2018
 *      Author: johan
 */

#include <csp/csp_debug.h>

#include <param/param.h>
#include <param_config.h>



PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_BUFFER_OUT,   csp_buf_out,         PARAM_TYPE_UINT8,  0, 0, PM_DEBUG | PM_ERRCNT, NULL, "", &csp_dbg_buffer_out, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_CONN_OUT,     csp_conn_out,        PARAM_TYPE_UINT8,  0, 0, PM_DEBUG | PM_ERRCNT, NULL, "", &csp_dbg_conn_out, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_CONN_OVF,     csp_conn_ovf,        PARAM_TYPE_UINT8,  0, 0, PM_DEBUG | PM_ERRCNT, NULL, "", &csp_dbg_conn_ovf, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_CONN_NOROUTE, csp_conn_noroute,    PARAM_TYPE_UINT8,  0, 0, PM_DEBUG | PM_ERRCNT, NULL, "", &csp_dbg_conn_noroute, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_INVAL_REPLY,  csp_inval_reply,     PARAM_TYPE_UINT8,  0, 0, PM_DEBUG | PM_ERRCNT, NULL, "", &csp_dbg_inval_reply, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_ERRNO,        csp_errno,           PARAM_TYPE_UINT8,  0, 0, PM_DEBUG, NULL, "", &csp_dbg_errno, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_CAN_ERRNO,    csp_can_errno,       PARAM_TYPE_UINT8,  0, 0, PM_DEBUG, NULL, "", &csp_dbg_can_errno, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_RDP_PRINT,    csp_print_rdp,       PARAM_TYPE_UINT8,  0, 0, PM_DEBUG, NULL, "", &csp_dbg_rdp_print, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_PACKET_PRINT, csp_print_packet,    PARAM_TYPE_UINT8,  0, 0, PM_DEBUG, NULL, "", &csp_dbg_packet_print, NULL);
