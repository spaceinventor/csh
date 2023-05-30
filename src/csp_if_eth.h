#pragma once

#include <csp/csp_interface.h>

// 0x88B5 : IEEE Std 802 - Local Experimental Ethertype
#define ETH_TYPE_CSP 0x6666

void csp_if_eth_init(csp_iface_t * iface, const char * device, const char * ifname, int mtu, bool promisc);
