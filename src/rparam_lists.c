/*
 * ax_client.c
 *
 *  Created on: Nov 21, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <param/rparam.h>
#include <param/rparam_list.h>
#include <param/rparam_listset.h>

rparam_listset_t list_basic = {
	.listname = "basic",
	.names = {
		"csp_node",
		"csp_can_speed",
		"csp_rtable",
		"gndwdt",
		"boot_alt",
		NULL,
	},
};

rparam_listset_t list_com_hk = {
	.listname = "com_hk",
	.names = {
		"rssi_bgnd",
		"rssi",
		"rfoff",
		"rx_count",
		"rx_err",
		"rx_auth_err",
		"rx_bytes",
		"tx_count",
		"tx_err",
		"tx_bytes",
		"tun_rx",
		"tun_tx",
		NULL,
	},
};

rparam_listset_t list_com_conf = {
	.listname = "com_conf",
	.names = {
		"conf_rx",
		"conf_tx",
		"rx_freq",
		"tx_freq",
		"confptr",
		"preamblen",
		"rx_guard",
		"tx_guard",
		"rssi_guard",
		"tx_max",
		"tx_inhibit",
		"tx_updly",
		"rssi_busy_thr",
		"rssi_ignore_thr",
		NULL,
	},
};

rparam_listset_t list_mppt_hk = {
	.listname = "mppt_hk",
	.names = {
		"mppt0_mode",
		"mppt0_vfixed",
		"v_in0",
		"i_in0",
		"v_out0",
		"i_out0",
		"mppt1_mode",
		"mppt1_vfixed",
		"v_in1",
		"i_in1",
		"v_out1",
		"i_out1",
		"temp",
		NULL,
	},
};

void rparam_lists_init(void) {
	rparam_listset_add(&list_basic);
	rparam_listset_add(&list_com_hk);
	rparam_listset_add(&list_com_conf);
	rparam_listset_add(&list_mppt_hk);
}
