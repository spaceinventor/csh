/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Space Inventor ApS
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <csp/csp_hooks.h>
#include <sys/types.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include "base16.h"
#include "known_hosts.h"

slash_command_group(csp, "Cubesat Space Protocol");

static int slash_csp_info(struct slash *slash)
{
#if (CSP_HAVE_STDIO)
#if (CSP_USE_RTABLE)
	csp_rtable_print();
#endif
	csp_conn_print_table();
	csp_iflist_print();
#endif
	return SLASH_SUCCESS;
}

slash_command(info, slash_csp_info, NULL, "Show CSP info");

static int slash_csp_ping(struct slash *slash)
{

	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int size = 0;

    optparse_t * parser = optparse_new("ping", "[node]");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
	optparse_add_unsigned(parser, 's', "size", "NUM", 0, &size, "size (default = 0)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	slash_printf(slash, "Ping node %u size %u timeout %u: ", node, size, timeout);

	int result = csp_ping(node, timeout, size, CSP_O_CRC32);

	if (result >= 0) {
		slash_printf(slash, "Reply in %d [ms]\n", result);
	} else {
		slash_printf(slash, "No reply\n");
	}

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(ping, slash_csp_ping, "[node]", "Ping a system");

static int slash_csp_reboot(struct slash *slash)
{

	unsigned int node = slash_dfl_node;

    optparse_t * parser = optparse_new("reboot", "[node]");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

   	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}


	csp_reboot(node);

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(reboot, slash_csp_reboot, "[node]", "Reboot a node");

static int slash_csp_shutdown(struct slash *slash)
{

	unsigned int node = slash_dfl_node;

    optparse_t * parser = optparse_new("shutdown", "[node]");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

   	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}


	csp_shutdown(node);

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(shutdown, slash_csp_shutdown, "[node]", "Shutdown a node");

static int slash_csp_buffree(struct slash *slash)
{

	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("buffree", "[node]");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

   	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}


	csp_buf_free(node, timeout);

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(buffree, slash_csp_buffree, "[node]", "");

static int slash_csp_uptime(struct slash *slash)
{

	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("uptime", "[node]");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

   	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}


	csp_uptime(node, timeout);

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(uptime, slash_csp_uptime, "[node]", "");

static int slash_csp_cmp_ident(struct slash *slash)
{
	
	
	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("ident", "[node]");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	struct csp_cmp_message msg;
	msg.type = CSP_CMP_REQUEST;
	msg.code = CSP_CMP_IDENT;
	int size = sizeof(msg.type) + sizeof(msg.code) + sizeof(msg.ident);

	csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, node, CSP_CMP, timeout, CSP_O_CRC32);
	if (conn == NULL) {
        optparse_del(parser);
		return 0;
	}

	csp_packet_t * packet = csp_buffer_get(size);
	if (packet == NULL) {
		csp_close(conn);
        optparse_del(parser);
		return 0;
	}

	/* Copy the request */
	memcpy(packet->data, &msg, size);
	packet->length = size;

	csp_send(conn, packet);

	while((packet = csp_read(conn, timeout)) != NULL) {
		memcpy(&msg, packet->data, packet->length < size ? packet->length : size);
		if (msg.code == CSP_CMP_IDENT) {
			printf("\nIDENT %hu\n", packet->id.src);
			printf("  %s\n  %s\n  %s\n  %s %s\n", msg.ident.hostname, msg.ident.model, msg.ident.revision, msg.ident.date, msg.ident.time);
			known_hosts_add(packet->id.src, msg.ident.hostname, false);
		}
		csp_buffer_free(packet);
	}

	csp_close(conn);

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(ident, slash_csp_cmp_ident, "[node]", "Ident");


static int slash_csp_cmp_ifstat(struct slash *slash)
{

	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("ifstat", "<ifname>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Expect ifname */
	if (++argi >= slash->argc) {
		printf("missing interface name\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * interface = slash->argv[argi];

	struct csp_cmp_message message;

	strncpy(message.if_stats.interface, interface, CSP_CMP_ROUTE_IFACE_LEN - 1);

	if (csp_cmp_if_stats(node, timeout, &message) != CSP_ERR_NONE) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	message.if_stats.tx =       be32toh(message.if_stats.tx);
	message.if_stats.rx =       be32toh(message.if_stats.rx);
	message.if_stats.tx_error = be32toh(message.if_stats.tx_error);
	message.if_stats.rx_error = be32toh(message.if_stats.rx_error);
	message.if_stats.drop =     be32toh(message.if_stats.drop);
	message.if_stats.autherr =  be32toh(message.if_stats.autherr);
	message.if_stats.frame =    be32toh(message.if_stats.frame);
	message.if_stats.txbytes =  be32toh(message.if_stats.txbytes);
	message.if_stats.rxbytes =  be32toh(message.if_stats.rxbytes);
	message.if_stats.irq = 	 be32toh(message.if_stats.irq);


	printf("%-5s   tx: %05"PRIu32" rx: %05"PRIu32" txe: %05"PRIu32" rxe: %05"PRIu32"\n"
	       "        drop: %05"PRIu32" autherr: %05"PRIu32" frame: %05"PRIu32"\n"
	       "        txb: %"PRIu32" rxb: %"PRIu32"\n\n",
		message.if_stats.interface,
		message.if_stats.tx,
		message.if_stats.rx,
		message.if_stats.tx_error,
		message.if_stats.rx_error,
		message.if_stats.drop,
		message.if_stats.autherr,
		message.if_stats.frame,
		message.if_stats.txbytes,
		message.if_stats.rxbytes);

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(ifstat, slash_csp_cmp_ifstat, "<node> <interface> [timeout]", "Ident");

static int slash_csp_cmp_peek(struct slash *slash)
{

	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("peek", "<addr> <len>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Expect address */
	if (++argi >= slash->argc) {
		printf("missing address\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * endptr;
	uint32_t address = strtoul(slash->argv[argi], &endptr, 16);
	if (*endptr != '\0') {
		printf("Failed to parse address\n");
        optparse_del(parser);
		return SLASH_EUSAGE;
	}

	
	/* Expect length */
	if (++argi >= slash->argc) {
		printf("missing length\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	unsigned int length = strtoul(slash->argv[argi], &endptr, 10);
	if (*endptr != '\0') {
		printf("Failed to parse length\n");
        optparse_del(parser);
		return SLASH_EUSAGE;
	}

	struct csp_cmp_message message;

	message.peek.addr = htobe32(address);
	message.peek.len = length;

	if (csp_cmp_peek(node, timeout, &message) != CSP_ERR_NONE) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	printf("Peek at address %p len %u\n", (void *) (intptr_t) address, length);
	csp_hex_dump(NULL, message.peek.data, length);

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(peek, slash_csp_cmp_peek, "<address> <len>", "Peek");

static int slash_csp_cmp_poke(struct slash *slash)
{
	
	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("poke", "<addr> <data base16>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Expect address */
	if (++argi >= slash->argc) {
		printf("missing address\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * endptr;
	uint32_t address = strtoul(slash->argv[argi], &endptr, 16);
	if (*endptr != '\0') {
		printf("Failed to parse address\n");
        optparse_del(parser);
		return SLASH_EUSAGE;
	}

	/* Expect data */
	if (++argi >= slash->argc) {
		printf("missing data\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}


	struct csp_cmp_message message;

	message.poke.addr = htobe32(address);

	int outlen = base16_decode(slash->argv[argi], (uint8_t *) message.poke.data);

	message.poke.len = outlen;

	printf("Poke at address %p\n", (void *) (intptr_t) address);
	csp_hex_dump(NULL, message.poke.data, outlen);

	if (csp_cmp_poke(node, timeout, &message) != CSP_ERR_NONE) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	printf("Poke ok\n");

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(poke, slash_csp_cmp_poke, "<address> <data>", "Poke");

static int slash_csp_cmp_time(struct slash *slash)
{
	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
	unsigned int timestamp = 0;
	int sync = 0;

    optparse_t * parser = optparse_new("time", "");
    optparse_add_help(parser);
	optparse_add_set(parser, 's', "sync", 1, &sync, "sync time with CSH");
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'T', "timestamp", "NUM", 0, &timestamp, "timestamp to configure in remote node)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (sync != 0 && timestamp != 0) {
		printf("You cannot sync with both a specific timestamp and the local time\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	struct csp_cmp_message message;

	csp_timestamp_t localtime;
	csp_clock_get_time(&localtime);

	if (sync) {
		message.clock.tv_sec = htobe32(localtime.tv_sec);
		message.clock.tv_nsec = htobe32(localtime.tv_nsec);
	} else {
		message.clock.tv_sec = htobe32(timestamp);
		message.clock.tv_nsec = htobe32(0);
	}

	if (csp_cmp_clock(node, timeout, &message) != CSP_ERR_NONE) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	message.clock.tv_sec = be32toh(message.clock.tv_sec);
	message.clock.tv_nsec = be32toh(message.clock.tv_nsec);

	int64_t remote_time_ns = message.clock.tv_sec * (uint64_t) 1E9 + message.clock.tv_nsec;
	int64_t local_time_ns = localtime.tv_sec * (uint64_t) 1E9 + localtime.tv_nsec;

	printf("Remote time is %"PRIu32".%09"PRIu32" (diff %"PRIi64" ms)\n", message.clock.tv_sec, message.clock.tv_nsec, (remote_time_ns - local_time_ns) / 1000000);

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(time, slash_csp_cmp_time, NULL, "Get or synchronize timestamp");
