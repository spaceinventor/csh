#include <stdio.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <unistd.h>

static int stdbuf2_mon_slash(struct slash *slash) {

    unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("stdbuf2", "");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

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

    while ((packet = csp_read(conn, timeout))) {
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
