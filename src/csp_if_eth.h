#pragma once

#include <csp/csp_interface.h>

// 0x88B5 : IEEE Std 802 - Local Experimental Ethertype
// Value in host order, so use htons(ETH_TYPE_CSP).
#define ETH_TYPE_CSP 0x88B5

void csp_if_eth_init(csp_iface_t * iface, const char * device, const char * ifname, int mtu);
