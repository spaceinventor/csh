#include "csp_if_eth.h"
#include <csp/csp.h>
#include <csp/csp_interface.h>
#include <csp/csp_id.h>

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <unistd.h>
#include <pthread.h>

bool eth_debug = false;

void eth_print(const char * title, uint8_t * data, size_t size)
{
    if (eth_debug) {
        printf("%s [%lu]\n", title, (unsigned long)size);
        for (size_t i = 0; i < size; ++i) {
            printf(" %02x", data[i]);
            if (i % 32 == 31) {
                printf("\n");
            }
        }
        if (size % 32 != 0) {
            printf("\n");
        }
        printf("\n");
    }
}


#define BUF_SIZ	2048

// Global variables assumes a SINGLE ethernet device
static int sockfd = -1;

static struct ifreq if_idx;
static struct ifreq if_mac;

static uint16_t tx_mtu = 0;

//PHM TEMP - Remove
void print_data(uint8_t * data, size_t size)
{
    printf("[%u] ", (unsigned)size);
    if (size && data) {
        for (size_t i = 0; i < size; ++i) {
            printf("0x%02x ", *(data + i));
        }
    }
    printf("\n");
}

void print_csp_packet(csp_packet_t * packet, const char * desc)
{
    printf("%s P{", desc);
    if (packet) {
        printf("ID{pri:%u,fl:%02x,S:%u,D:%u,Sp:%u,Dp:%u}",
               (unsigned)(packet->id.pri), (unsigned)(packet->id.flags), 
               (unsigned)(packet->id.src), (unsigned)(packet->id.dst), 
               (unsigned)(packet->id.sport), (unsigned)(packet->id.dport));
        printf(",len:%u", (unsigned)packet->length);
    } else {
        printf("0");
    }
    printf("}\n");
    print_data((uint8_t*)packet->frame_begin, packet->frame_length);
}

#define DEB printf("%s:%d\n", __FILE__, __LINE__);

typedef struct arp_list_entry_s {
    uint16_t csp_addr;
    uint8_t mac_addr[ETH_ALEN];
    struct arp_list_entry_s * next;
} arp_list_entry_t;

static arp_list_entry_t * arp_list = 0; 

void arp_print()
{
    printf("ARP  CSP  MAC\n");
    for (arp_list_entry_t * arp = arp_list; arp; arp = arp->next) {
        printf("     %3u  ", (unsigned)(arp->csp_addr));
        for (int i = 0; i < ETH_ALEN; ++i) {
            printf("%02x ", arp->mac_addr[i]);
        }
    }
    printf("\n");
}

void arp_set_addr(uint16_t csp_addr, uint8_t * mac_addr) 
{
    arp_list_entry_t * last_arp = 0;
    for (arp_list_entry_t * arp = arp_list; arp; arp = arp->next) {
        last_arp = arp;
        if (arp->csp_addr == csp_addr) {
            // Already set
            return;
        }
    }

    // Create and add a new ARP entry
    arp_list_entry_t * new_arp = malloc(sizeof(arp_list_entry_t));
    new_arp->csp_addr = csp_addr;
	memcpy(new_arp->mac_addr, mac_addr, ETH_ALEN);
    new_arp->next = 0;

    if (last_arp) {
        last_arp->next = new_arp;
    } else {
        arp_list = new_arp;
    }
}

void arp_get_addr(uint16_t csp_addr, uint8_t * mac_addr) 
{
    for (arp_list_entry_t * arp = arp_list; arp ; arp = arp->next) {
        if (arp->csp_addr == csp_addr) {
            memcpy(mac_addr, arp->mac_addr, ETH_ALEN);
            return;
        }
    }
    // Defaults to returning the broadcast address
	memset(mac_addr, 0xff, ETH_ALEN);
}


/**
 * lwip checksum
 *
 * @param dataptr points to start of data to be summed at any boundary
 * @param len length of data to be summed
 * @return host order (!) lwip checksum (non-inverted Internet sum)
 *
 * @note accumulator size limits summable length to 64k
 * @note host endianness is irrelevant (p3 RFC1071)
 */
uint16_t lwip_standard_chksum(const void *dataptr, int len) {
    uint32_t acc;
    uint16_t src;
    const uint8_t *octetptr;

    acc = 0;
    /* dataptr may be at odd or even addresses */
    octetptr = (const uint8_t *)dataptr;
    while (len > 1) {
        /* declare first octet as most significant
       thus assume network order, ignoring host order */
        src = (*octetptr) << 8;
        octetptr++;
        /* declare second octet as least significant */
        src |= (*octetptr);
        octetptr++;
        acc += src;
        len -= 2;
    }
    if (len > 0) {
        /* accumulate remaining octet */
        src = (*octetptr) << 8;
        acc += src;
    }
    /* add deferred carry bits */
    acc = (acc >> 16) + (acc & 0x0000ffffUL);
    if ((acc & 0xffff0000UL) != 0) {
        acc = (acc >> 16) + (acc & 0x0000ffffUL);
    }
    /* This maybe a little confusing: reorder sum using lwip_htons()
     instead of lwip_ntohs() since it has a little less call overhead.
     The caller must invert bits for Internet sum ! */
    return htons((uint16_t)acc);
}


static int csp_if_eth_tx(csp_iface_t * iface, uint16_t via, csp_packet_t * packet, int from_me) {
	/* Loopback */
	if (packet->id.dst == iface->addr) {
		csp_qfifo_write(packet, iface, NULL);
		return CSP_ERR_NONE;
	}

	csp_id_prepend(packet);

    if (packet->frame_length > tx_mtu) {
        iface->rx_error++;
        csp_buffer_free(packet);
		return CSP_ERR_DRIVER;
    }

	/* Construct the Ethernet header */
	uint8_t sendbuf[BUF_SIZ];
    memset(sendbuf, 0, BUF_SIZ);

	/* Ethernet header */
    struct ether_header *eh = (struct ether_header *) sendbuf;
	uint16_t head_size = sizeof(struct ether_header);

    memcpy(eh->ether_shost, if_mac.ifr_hwaddr.sa_data, ETH_ALEN);
 
    arp_get_addr(packet->id.dst, (uint8_t*)(eh->ether_dhost)); 

	eh->ether_type = htons(ETH_TYPE_CSP);

	/* Destination socket address */
	struct sockaddr_ll socket_address = {};
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
	socket_address.sll_halen = ETH_ALEN;
    memcpy(socket_address.sll_addr, eh->ether_dhost, ETH_ALEN);

    sendbuf[head_size] = packet->frame_length / 256;
    sendbuf[head_size + 1] = packet->frame_length % 256;

    memcpy(&sendbuf[head_size + 2], packet->frame_begin, packet->frame_length);
    eth_print("tx", sendbuf, head_size + 2 + packet->frame_length);
    if (sendto(sockfd, sendbuf, head_size + 2 + packet->frame_length, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0) {
        csp_buffer_free(packet);
		return CSP_ERR_DRIVER;
    }

    csp_buffer_free(packet);
    return CSP_ERR_NONE;
}


void csp_if_eth_rx_loop(void * param) {

    static csp_iface_t * iface;
    iface = param;

    while(1) {

        csp_packet_t * packet = csp_buffer_get(0);
        if (packet == NULL) {
            usleep(10000);
            continue;
        }

		csp_id_setup_rx(packet);

        /* Data buffer */
        uint8_t recvbuf[BUF_SIZ];

        /* Ethernet header */
        struct ether_header * eh = (struct ether_header *)recvbuf;
    	uint16_t head_size = sizeof(struct ether_header);
        eh->ether_type = ntohs(eh->ether_type);

        // Receive 
        int received_len = recvfrom(sockfd, recvbuf, BUF_SIZ, 0, NULL, NULL);
        eth_print("rx", recvbuf, received_len);

        /* Filter : ether head (14) + packet length + CSP head */
        if (received_len < head_size + 2 + 6) {
            iface->rx_error++;
            csp_buffer_free(packet);
            continue;
        }

        /* Filter on CSP protocol id */
        if (eh->ether_type != ETH_TYPE_CSP) {
            iface->rx_error++;
            csp_buffer_free(packet);
            continue;
        }

        packet->frame_length = (uint16_t)(recvbuf[head_size]) * 256 + recvbuf[head_size + 1];

        if (packet->frame_length > received_len - head_size) {
            iface->rx_error++;
            csp_buffer_free(packet);
            continue;
        }

        memcpy(packet->frame_begin, &recvbuf[head_size + 2], packet->frame_length);

        /* Parse the frame and strip the ID field */
        if (csp_id_strip(packet) != 0) {
            iface->rx_error++;
            csp_buffer_free(packet);
            continue;
        }

        // Record (CSP,MAC) addresses of source
        arp_set_addr(packet->id.src, eh->ether_shost);

        csp_qfifo_write(packet, iface, NULL);
	    
    }

}

void csp_if_eth_init(csp_iface_t * iface, const char * device, const char * ifname, int mtu) {

    /**
     * TX SOCKET
     */

    /* Open RAW socket to send on */
	if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(0x6666))) == -1) {
	    perror("socket");
        return;
	}

	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifname, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
	    perror("SIOCGIFINDEX");
        return;
    }

	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifname, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
	    perror("SIOCGIFHWADDR");
        return;
    }

	printf("ETH %s idx %d mac %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n", ifname, if_idx.ifr_ifindex, 
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0],
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1],
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2],
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3],
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4],
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5]);

#if 0
	/* Set interface to promiscuous mode - do we need to do this every time? */
	/* Answer: no this is not needed */
    struct ifreq ifopts = {};	/* set promiscuous mode */
	strncpy(ifopts.ifr_name, ifname, IFNAMSIZ-1);
	ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
	ifopts.ifr_flags |= IFF_PROMISC;
	ioctl(sockfd, SIOCSIFFLAGS, &ifopts);
#endif

    /* Allow the socket to be reused - incase connection is closed prematurely */
    int sockopt;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1) {
		perror("setsockopt");
		close(sockfd);
		return;
	}

	/* Bind to device */
	if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, ifname, IFNAMSIZ-1) == -1)	{
		perror("SO_BINDTODEVICE");
		close(sockfd);
		return;
	}

	/* fill sockaddr_ll struct to prepare binding */
	struct sockaddr_ll my_addr;
	my_addr.sll_family = AF_PACKET;
	my_addr.sll_protocol = htons(ETH_TYPE_CSP);
	my_addr.sll_ifindex = if_idx.ifr_ifindex;

	/* bind socket  */
	bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_ll));

    tx_mtu = mtu;

	/* Start server thread */
	void * csp_if_eth_rx_task(void * param) {
		csp_if_eth_rx_loop(param);
		return NULL;
	}
    static pthread_t server_handle;
	pthread_create(&server_handle, NULL, &csp_if_eth_rx_task, iface);
usleep(1);

    /**
     * CSP INTERFACE
     */

	/* Regsiter interface */
	iface->name = ifname,
	iface->nexthop = &csp_if_eth_tx,
	csp_iflist_add(iface);
DEB
}

