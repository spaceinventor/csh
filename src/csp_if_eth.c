#include <csp/csp.h>
#include <csp/csp_interface.h>
#include <csp/csp_id.h>
#include <csp/arch/csp_thread.h>

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

#define BUF_SIZ	2048

static int sockfd;
static struct ifreq if_idx;
static struct ifreq if_mac;

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

static int csp_if_eth_tx(const csp_route_t * ifroute, csp_packet_t * packet) {

	csp_id_prepend(packet);

	/* Construct the Ethernet header */
	char sendbuf[BUF_SIZ];
    memset(sendbuf, 0, BUF_SIZ);

	/* Ethernet header */
    struct ether_header *eh = (struct ether_header *) sendbuf;
	int tx_len = sizeof(struct ether_header);

	/* We just broadcast on ethernet! */
	eh->ether_shost[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
	eh->ether_shost[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
	eh->ether_shost[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
	eh->ether_shost[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
	eh->ether_shost[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
	eh->ether_shost[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];
	eh->ether_dhost[0] = 0xff;
	eh->ether_dhost[1] = 0xff;
	eh->ether_dhost[2] = 0xff;
	eh->ether_dhost[3] = 0xff;
	eh->ether_dhost[4] = 0xff;
	eh->ether_dhost[5] = 0xff;
#if 1
	eh->ether_type = htons(0x6666);

#else
	eh->ether_type = htons(0x0800);
    
	/* IP header */
	struct iphdr *iph = (struct iphdr *) (sendbuf + tx_len);
	tx_len += sizeof(struct iphdr);

	iph->version = 4;
	iph->ihl = 5;
	iph->tos = 0;
	iph->id = 0;
	iph->frag_off = 0;
	iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + packet->frame_length);
	iph->ttl = 10;
	iph->protocol = 17; // UDP
	iph->saddr = htonl(0x0A000000 | packet->id.src);
	iph->daddr = htonl(0x0A000000 | packet->id.dst);
	iph->check = 0;
	iph->check = ~lwip_standard_chksum(iph, sizeof(struct iphdr));

	/* UDP header */
	struct udphdr *udp = (struct udphdr *) (sendbuf + tx_len);
	tx_len += sizeof(struct udphdr);

	udp->source = htons(9000);
	udp->dest = htons(9000);
	udp->len = htons(sizeof(struct udphdr) + packet->frame_length);
	udp->check = 0; // TODO;
#endif

    /* Copy data to outgoing */
    memcpy(&sendbuf[tx_len], packet->frame_begin, packet->frame_length);
    tx_len += packet->frame_length;

	/* Socket address */
	struct sockaddr_ll socket_address = {};
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
	socket_address.sll_halen = ETH_ALEN;
	socket_address.sll_addr[0] = eh->ether_dhost[0];
	socket_address.sll_addr[1] = eh->ether_dhost[1];
	socket_address.sll_addr[2] = eh->ether_dhost[2];
	socket_address.sll_addr[3] = eh->ether_dhost[3];
	socket_address.sll_addr[4] = eh->ether_dhost[4];
	socket_address.sll_addr[5] = eh->ether_dhost[5];

	/* Send packet */
	if (sendto(sockfd, sendbuf, tx_len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
	    printf("Send failed\n");

    csp_buffer_free(packet);

    return CSP_ERR_NONE;

}


CSP_DEFINE_TASK(csp_if_eth_rx_task) {

    static csp_iface_t * iface;
    iface = param;

    while(1) {

        csp_packet_t * packet = csp_buffer_get(0);
        if (packet == NULL) {
            csp_sleep_ms(10);
            continue;
        }

        /* Setup RX frane to point to ID */
        int csp_header_size = csp_id_setup_rx(packet);
        uint8_t * eth_frame_begin = packet->frame_begin - 14;
        int received_len = recvfrom(sockfd, eth_frame_begin, iface->mtu + 14 + csp_header_size, 0, NULL, NULL);
        packet->frame_length = received_len - 14;

        /* Filter */
        if (received_len < 14) {
            iface->rx_error++;
            csp_buffer_free(packet);
            continue;
        }

        /* Filter */
        if ((eth_frame_begin[12] != 0x66) || (eth_frame_begin[13] != 0x66)) {
            iface->rx_error++;
            csp_buffer_free(packet);
            continue;
        }

        /* Parse the frame and strip the ID field */
        if (csp_id_strip(packet) != 0) {
            iface->rx_error++;
            csp_buffer_free(packet);
            continue;
        }

        csp_qfifo_write(packet, iface, NULL);
	    
    }

}

void csp_if_eth_init(csp_iface_t * iface, char * ifname) {

    /**
     * TX SOCKET
     */

    /* Open RAW socket to send on */
	if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) == -1) {
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

	/* Set interface to promiscuous mode - do we need to do this every time? */
    struct ifreq ifopts = {};	/* set promiscuous mode */
	strncpy(ifopts.ifr_name, ifname, IFNAMSIZ-1);
	ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
	ifopts.ifr_flags |= IFF_PROMISC;
	ioctl(sockfd, SIOCSIFFLAGS, &ifopts);

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
	my_addr.sll_protocol = htons(ETH_P_ALL);
	my_addr.sll_ifindex = if_idx.ifr_ifindex;

	/* bind socket  */
	bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_ll));

	/* Start server thread */
    static csp_thread_handle_t server_handle;
	int ret = csp_thread_create(csp_if_eth_rx_task, "ETH", 10000, iface, 0, &server_handle);
	csp_log_info("csp_if_eth_rx_task start %d\r\n", ret);

    /**
     * CSP INTERFACE
     */

	/* MTU is datasize */
	iface->mtu = 1500;

	/* Regsiter interface */
	iface->name = "ETH",
	iface->nexthop = &csp_if_eth_tx,
	csp_iflist_add(iface);

}

