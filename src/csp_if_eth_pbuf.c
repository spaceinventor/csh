#include "csp_if_eth_pbuf.h"
#include <csp/csp.h>
#include <csp/csp_interface.h>
#include <csp/csp_id.h>
#include <csp/arch/csp_time.h>


uint16_t csp_if_eth_pbuf_pack_head(uint8_t * buf, 
                                   uint16_t packet_id, uint16_t src_addr,
                                   uint16_t seg_size, uint16_t packet_length) {

    buf[0] = packet_id / 256;
    buf[1] = packet_id % 256;
    buf[2] = src_addr / 256;
    buf[3] = src_addr % 256;
    buf[4] = seg_size / 256;
    buf[5] = seg_size % 256;
    buf[6] = packet_length / 256;
    buf[7] = packet_length % 256;
    return 8;

}

uint16_t csp_if_eth_pbuf_unpack_head(uint8_t * buf, 
                                     uint16_t * packet_id, uint16_t * src_addr,
                                     uint16_t * seg_size, uint16_t * packet_length) {

    if (packet_id) {
        *packet_id = (uint16_t)buf[0] * 256 + buf[1];
    }
    if (src_addr) {
        *src_addr = (uint16_t)buf[2] * 256 + buf[3];
    }
    if (seg_size) {
        *seg_size = (uint16_t)buf[4] * 256 + buf[5];
    }
    if (packet_length) {
        *packet_length = (uint16_t)buf[6] * 256 + buf[7];
    }
    return 8;

}

uint32_t csp_if_eth_pbuf_id_as_int32(uint8_t * buf) {

    return *((uint32_t*)buf);

}


/** Packet buffer list operations */

/** Differences between POSIX and FreeRTOS 
 * POSIX: rx handler performs all, so no protected regions are required
 * FreeRTOS: The isr enqueues received packets, while task dequeues and forward to CSP
 *           The task must disable interrupt so isr does not interrupt within the region.
 *           The isr only allocates and enqueues new packets.
*/

#ifdef portENTER_CRITICAL
    #define ENTER_REGION portENTER_CRITICAL();
#else
    #define ENTER_REGION
#endif

#ifdef portEXIT_CRITICAL
    #define EXIT_REGION portEXIT_CRITICAL();
#else
    #define EXIT_REGION
#endif

csp_packet_t * csp_if_eth_pbuf_find(csp_packet_t ** plist, uint32_t pbuf_id) {

    csp_packet_t * packet = *plist;
    while(packet) {
        if (packet->cfpid == pbuf_id) {
            return packet;
        }
        packet = packet->next;
    }    
    return packet;

}

void csp_if_eth_pbuf_insert(csp_packet_t ** plist, csp_packet_t * packet) {

    if (*plist) {
        packet->next = *plist;
    }
    *plist = packet;

}

csp_packet_t * csp_if_eth_pbuf_get(csp_packet_t ** plist, uint32_t pbuf_id, bool isr) {

    csp_packet_t * packet = csp_if_eth_pbuf_find(plist, pbuf_id);

    if (packet) {
    	packet->last_used = csp_get_ms();
        return packet;
    }

    while (!packet) {
        packet = isr ? csp_buffer_het_isr(0) : csp_buffer_get(0);
        if (!packet) {
            /* No free packet */
            usleep(10000);
        }
    }

	csp_id_setup_rx(packet);

    /* Existing cfpid and rx_count fields are used */ 
    packet->cfpid = pbuf_id;
    packet->rx_count = 0;
	packet->last_used = isr ? csp_get_ms_isr() : csp_get_ms();

    packet->next = 0;
    csp_if_eth_pbuf_insert(plist, packet);

    return packet;

}

void csp_if_eth_pbuf_remove(csp_packet_t ** plist, csp_packet_t * packet) {

    csp_packet_t * prev = 0;
    csp_packet_t * p = *plist;
    while(p && (p != packet)) {
        prev = p;
        p = p->next;
    }   

    if (p) {
        if (prev) {
            prev->next = p->next;
        } else {
            *plist = p->next;
        }
    }

}

void csp_if_eth_pbuf_list_cleanup(csp_packet_t ** plist, bool is_isr) {

    /* Free stalled packets, like for which a segment has been lost */
    uint32_t now = csp_get_ms();
    csp_packet_t * packet = *plist;
    while(packet) {
		if (now > packet->last_used + CSP_IF_ETH_PBUF_TIMEOUT_MS) {
            csp_if_eth_pbuf_print("timeout ", packet);
            csp_if_eth_pbuf_remove(plist, packet);
            csp_buffer_free(packet);
        }
        packet = packet->next;
    }

}

void csp_if_eth_pbuf_print(const char * descr, csp_packet_t * packet) {

    if (packet) {
        printf("%s %p id:%u Age:%lu,%lu,%lu rx:%u  flen:%u\n",
            descr, packet, (unsigned)packet->cfpid, (unsigned long)csp_get_ms(), (unsigned long)packet->last_used, (unsigned long)(csp_get_ms() - packet->last_used), (unsigned)packet->rx_count, (unsigned)packet->frame_length);
    } else {
        printf("Packet is null\n");
    }

}

void csp_if_eth_pbuf_list_print(csp_packet_t ** plist) {

    csp_packet_t * packet = *plist;
    while(packet) {
        csp_if_eth_pbuf_print("list ", packet);
        packet = packet->next;
    }

}

