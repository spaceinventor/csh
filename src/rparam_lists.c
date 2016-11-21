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

rparam_listset_t list_hk = {
	.listname = "hk",
	.names = {
		"rssi_bgnd",
		"rx_count",
		"rx_err",
		"rx_auth_err",
		"rx_bytes",
		"tx_count",
		"tx_err",
		"tx_bytes",
		NULL,
	},
};

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

rparam_listset_t list_conf = {
	.listname = "conf",
	.names = {
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
		NULL,
	},
};

void rparam_lists_init(void) {
	rparam_listset_add(&list_hk);
	rparam_listset_add(&list_basic);
	rparam_listset_add(&list_conf);
}
