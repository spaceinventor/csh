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
#define PROTOCOL 0x6666

static int tx_sockfd;
static int rx_sockfd;
static struct ifreq if_idx;
static struct ifreq if_mac;

static int csp_if_eth_tx(const csp_route_t * ifroute, csp_packet_t * packet) {

	csp_id_prepend(packet);

	/* Construct the Ethernet header */
	char sendbuf[BUF_SIZ];
    memset(sendbuf, 0, BUF_SIZ);

	/* Ethernet header */
    struct ether_header *eh = (struct ether_header *) sendbuf;
	eh->ether_shost[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
	eh->ether_shost[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
	eh->ether_shost[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
	eh->ether_shost[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
	eh->ether_shost[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
	eh->ether_shost[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];
    #if 1
	eh->ether_dhost[0] = 0x66;
	eh->ether_dhost[1] = 0x66;
	eh->ether_dhost[2] = 0x66;
	eh->ether_dhost[3] = 0x66;
	eh->ether_dhost[4] = (packet->id.dst >> 8) & 0xFF;
	eh->ether_dhost[5] = (packet->id.dst) & 0xFF;
    #else
    eh->ether_dhost[0] = 0x04;
	eh->ether_dhost[1] = 0x33;
	eh->ether_dhost[2] = 0xc2;
	eh->ether_dhost[3] = 0x24;
	eh->ether_dhost[4] = 0x51;
	eh->ether_dhost[5] = 0x7b;
    #endif
	eh->ether_type = htons(PROTOCOL);

    int tx_len = sizeof(struct ether_header);

    /* Copy data to outgoing */
    memcpy(&sendbuf[tx_len], packet->frame_begin, packet->frame_length);
    tx_len += packet->frame_length;

	/* Index of the network device */
	struct sockaddr_ll socket_address = {};
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
    socket_address.sll_protocol = htons(PROTOCOL);
	/* Address length*/
	socket_address.sll_halen = ETH_ALEN;
	/* Destination MAC */
	socket_address.sll_addr[0] = eh->ether_dhost[0];
	socket_address.sll_addr[1] = eh->ether_dhost[1];
	socket_address.sll_addr[2] = eh->ether_dhost[2];
	socket_address.sll_addr[3] = eh->ether_dhost[3];
	socket_address.sll_addr[4] = eh->ether_dhost[4];
	socket_address.sll_addr[5] = eh->ether_dhost[5];

	/* Send packet */
	if (sendto(tx_sockfd, sendbuf, tx_len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
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
        int received_len = recvfrom(rx_sockfd, eth_frame_begin, iface->mtu + 14 + csp_header_size, 0, NULL, NULL);
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
	if ((tx_sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
	    perror("socket");
        return;
	}

	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifname, IFNAMSIZ-1);
	if (ioctl(tx_sockfd, SIOCGIFINDEX, &if_idx) < 0) {
	    perror("SIOCGIFINDEX");
        return;
    }

	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifname, IFNAMSIZ-1);
	if (ioctl(tx_sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
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

    /**
     * RX SOCKET
     */

    /* Open PF_PACKET socket, listening for EtherType ETHER_TYPE */
	if ((rx_sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
		perror("listener: socket");	
		return;
	}

	/* Set interface to promiscuous mode - do we need to do this every time? */
    struct ifreq ifopts = {};	/* set promiscuous mode */
	strncpy(ifopts.ifr_name, ifname, IFNAMSIZ-1);
	ioctl(rx_sockfd, SIOCGIFFLAGS, &ifopts);
	ifopts.ifr_flags |= IFF_PROMISC;
	ioctl(rx_sockfd, SIOCSIFFLAGS, &ifopts);

    /* Allow the socket to be reused - incase connection is closed prematurely */
    int sockopt;
	if (setsockopt(rx_sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1) {
		perror("setsockopt");
		close(rx_sockfd);
		return;
	}
	/* Bind to device */
	if (setsockopt(rx_sockfd, SOL_SOCKET, SO_BINDTODEVICE, ifname, IFNAMSIZ-1) == -1)	{
		perror("SO_BINDTODEVICE");
		close(rx_sockfd);
		return;
	}

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

