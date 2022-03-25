#include <stdio.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <slash/slash.h>
#include <unistd.h>

static int stdbuf2_mon_slash(struct slash *slash) {

	if (slash->argc < 2)
		return SLASH_EUSAGE;

	uint16_t node = atoi(slash->argv[1]);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, 15, 0, CSP_O_CRC32);
	if (conn == NULL)
		return SLASH_ENOMEM;

    csp_packet_t * packet = csp_buffer_get(1);
    if (packet == NULL) {
        csp_close(conn);
        return SLASH_ENOMEM;
    }
    packet->data[0] = 0xAA;
    packet->length = 1;
    csp_send(conn, packet);

    while ((packet = csp_read(conn, 1000))) {
        //csp_hex_dump("stdbuf", &packet->data[1], packet->length - 1);
        int ignore __attribute__((unused)) = write(fileno(stdout), &packet->data[1], packet->length - 1);

        int again = packet->data[0];
        csp_buffer_free(packet);

        if (again == 0) {
            break;
        }
    }

	csp_close(conn);

	return SLASH_SUCCESS;
}

slash_command(stdbuf2, stdbuf2_mon_slash, NULL, "Monitor stdbuf");
