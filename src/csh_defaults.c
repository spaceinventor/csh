#include <csp/csp.h>
#include <apm/csh_api.h>

unsigned int slash_dfl_node = 0;
unsigned int slash_dfl_timeout = 1000;
unsigned int rdp_dfl_window = 3;
unsigned int rdp_dfl_conn_timeout = 10000;
unsigned int rdp_dfl_packet_timeout = 5000;
unsigned int rdp_dfl_delayed_acks = 1;
unsigned int rdp_dfl_ack_timeout = 2000;
unsigned int rdp_dfl_ack_count = 2;

unsigned int rdp_tmp_window;
unsigned int rdp_tmp_conn_timeout;
unsigned int rdp_tmp_packet_timeout;
unsigned int rdp_tmp_delayed_acks;
unsigned int rdp_tmp_ack_timeout;
unsigned int rdp_tmp_ack_count;

void rdp_opt_set(void) {

	csp_rdp_set_opt(rdp_tmp_window, rdp_tmp_conn_timeout, rdp_tmp_packet_timeout, rdp_tmp_delayed_acks, rdp_tmp_ack_timeout, rdp_tmp_ack_count);

	printf("Using RDP options window: %u, conn_timeout: %u, packet_timeout: %u, ack_timeout: %u, ack_count: %u\n",
        rdp_tmp_window, rdp_tmp_conn_timeout, rdp_tmp_packet_timeout, rdp_tmp_ack_timeout, rdp_tmp_ack_count);
}

void rdp_opt_reset(void) {

	csp_rdp_set_opt(rdp_dfl_window, rdp_dfl_conn_timeout, rdp_dfl_packet_timeout, rdp_dfl_delayed_acks, rdp_dfl_ack_timeout, rdp_dfl_ack_count);
}
