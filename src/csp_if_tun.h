/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef SRC_CSP_IF_TUN_H_
#define SRC_CSP_IF_TUN_H_

#include <csp/csp.h>

typedef struct {

	/* Should be set before calling if_tun_init */
	int tun_src;
	int tun_dst;

} csp_if_tun_conf_t;

void csp_if_tun_init(csp_iface_t * iface, csp_if_tun_conf_t * ifconf);

#endif /* SRC_CSP_IF_TUN_H_ */
