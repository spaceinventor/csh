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

#include "csp_if_tun.h"
#include "crypto_test.h"
#include <csp/csp.h>

void csp_id_prepend(csp_packet_t * packet);
int csp_id_strip(csp_packet_t * packet);
int csp_id_setup_rx(csp_packet_t * packet);

static int csp_if_tun_tx(const csp_route_t * ifroute, csp_packet_t * packet) {

	csp_if_tun_conf_t * ifconf = ifroute->iface->driver_data;

	/* Allocate new frame */
	csp_packet_t * new_packet = csp_buffer_get(packet->frame_length);
	if (new_packet == NULL) {
		csp_buffer_free(packet);
		return CSP_ERR_NONE;
	}

	if (packet->id.dst == ifconf->tun_src) {

		/**
		 * Incomming tunnel packet
		 */
		csp_hex_dump("incoming packet", packet->data, packet->length);

		csp_id_setup_rx(new_packet);

#if 1
		int length = crypto_decrypt_with_zeromargin(packet->data, packet->length, new_packet->frame_begin);
		if (length < 0) {
			csp_buffer_free(new_packet);
			csp_buffer_free(packet);
			ifroute->iface->rx_error++;
			printf("Decryption error\n");
			return CSP_ERR_NONE;
		} else {
			new_packet->frame_length = length;
		}
#else
		/* Decapsulate */
		memcpy(new_packet->frame_begin, packet->data, packet->length);
		new_packet->frame_length = packet->length;
#endif

		csp_hex_dump("new frame", new_packet->frame_begin, new_packet->frame_length + 16);

		csp_id_strip(new_packet);

		csp_hex_dump("new packet", new_packet->data, new_packet->length);

		/* Send new packet */
		csp_qfifo_write(new_packet, ifroute->iface, NULL);

	} else {

		/**
		 * Outgoing tunnel packet
		 */

		csp_hex_dump("packet", packet->data, packet->length);

		/* Apply CSP header */
		csp_id_prepend(packet);

		csp_hex_dump("frame", packet->frame_begin, packet->frame_length);

		/* Create tunnel header */
		new_packet->id.dst = ifconf->tun_dst;
		new_packet->id.src = ifconf->tun_dst;
		new_packet->id.sport = 0;
		new_packet->id.dport = 0;
		new_packet->id.pri = packet->id.pri;
		new_packet->length = packet->frame_length;

#if 1
		/* Encrypt */
		new_packet->length = crypto_encrypt_with_zeromargin(packet->frame_begin, packet->frame_length, new_packet->data);
#else
		/* Encapsulate */
		memcpy(new_packet->data, packet->frame_begin, packet->frame_length);
#endif

		/* Free old packet */
		csp_buffer_free(packet);

		csp_hex_dump("new packet", new_packet->data, new_packet->length);

		/* Apply CSP header */
		csp_id_prepend(new_packet);

		csp_hex_dump("new frame", new_packet->frame_begin, new_packet->frame_length);

		/* Send new packet */
		csp_qfifo_write(new_packet, ifroute->iface, NULL);

	}

	return CSP_ERR_NONE;

}


void csp_if_tun_init(csp_iface_t * iface, csp_if_tun_conf_t * ifconf) {

	iface->driver_data = ifconf;

	printf("Setup Tunnel between %d and %d\n", ifconf->tun_src, ifconf->tun_dst);

	/* MTU is datasize */
	iface->mtu = csp_buffer_data_size();

	/* Regsiter interface */
	iface->name = "TUN",
	iface->nexthop = csp_if_tun_tx,
	csp_iflist_add(iface);

}

