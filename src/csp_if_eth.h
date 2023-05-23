#pragma once

#include <csp/csp_interface.h>

void csp_if_eth_init(csp_iface_t * iface, const char * device, const char * ifname, int mtu);