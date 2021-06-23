#include <csp/csp.h>
#include <csp/csp_interface.h>
#include <csp/csp_id.h>

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>

#define BUF_SIZ		2048

static int sockfd;
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
	eh->ether_dhost[0] = 0x66;
	eh->ether_dhost[1] = 0x66;
	eh->ether_dhost[2] = 0x66;
	eh->ether_dhost[3] = 0x66;
	eh->ether_dhost[4] = (packet->id.dst >> 8) & 0xFF;
	eh->ether_dhost[5] = (packet->id.dst) & 0xFF;
	eh->ether_type = htons(0x6666);

    int tx_len = sizeof(struct ether_header);

    /* Copy data to outgoing */
    memcpy(&sendbuf[tx_len], packet->frame_begin, packet->frame_length);
    tx_len += packet->frame_length;

	/* Index of the network device */
	struct sockaddr_ll socket_address;
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
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
	if (sendto(sockfd, sendbuf, tx_len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
	    printf("Send failed\n");

    return CSP_ERR_NONE;

}

void csp_if_eth_init(csp_iface_t * iface, char * ifname) {

    /* Open RAW socket to send on */
	if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
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

	/* Start server thread */
	//int ret = csp_thread_create(csp_if_udp_rx_task, "UDPS", 10000, iface, 0, &ifconf->server_handle);
	//csp_log_info("csp_if_udp_rx_task start %d\r\n", ret);

	/* MTU is datasize */
	iface->mtu = csp_buffer_data_size();

	/* Regsiter interface */
	iface->name = "ETH",
	iface->nexthop = &csp_if_eth_tx,
	csp_iflist_add(iface);

}

