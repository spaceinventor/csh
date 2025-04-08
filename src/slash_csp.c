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
#include <time.h>
#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <csp/csp_hooks.h>
#include <sys/types.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <apm/csh_api.h>
#include "base16.h"


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
    int override = false;

    optparse_t * parser = optparse_new("ident", "[node]");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
	optparse_add_set(parser, 'o', "override", true, &override, "Allow overriding hostname for a known node");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi < slash->argc) {
		node = atoi(slash->argv[argi]);
	}

	struct csp_cmp_message msg = {
		.type = CSP_CMP_REQUEST,
		.code = CSP_CMP_IDENT,
	};
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
			known_hosts_add(packet->id.src, msg.ident.hostname, override);
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
	unsigned int version = 1;

    optparse_t * parser = optparse_new("peek", "<addr> <len>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version, 1=32-bit <addr>, 2=64-bit <addr> (default = 1)");

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
	uint64_t address = strtoull(slash->argv[argi], &endptr, 16);
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

	if (version < 1 || version > 2) {
		printf("Unsupported version: %d, only supports 1 (32-bit) and 2 (64-bit)\n", version);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	struct csp_cmp_message message;

	switch (version) {
		case 1:
		{
			if (address > 0x00000000FFFFFFFFULL) {
				printf("Peek address out of 32-bit addressing range for version 1, try version 2.\n");
				optparse_del(parser);
				return SLASH_EINVAL;
			}

			if(length > CSP_CMP_PEEK_MAX_LEN){
				printf("Max peek length %u\n", CSP_CMP_PEEK_MAX_LEN);
				optparse_del(parser);
				return SLASH_EUSAGE;
			}

			message.peek.addr = htobe32(address);
			message.peek.len = length;

			if (csp_cmp_peek(node, timeout, &message) != CSP_ERR_NONE) {
				printf("No response\n");
				optparse_del(parser);
				return SLASH_EINVAL;
			}

			printf("Peek at address 0x%"PRIx32" len %u\n", (uint32_t) address, length);
			csp_hex_dump(NULL, message.peek.data, length);
		}
		break;
		case 2:
		{
			if(length > CSP_CMP_PEEK_V2_MAX_LEN){
				printf("Max peek length %u\n", CSP_CMP_PEEK_V2_MAX_LEN);
				optparse_del(parser);
				return SLASH_EUSAGE;
			}

			message.peek_v2.vaddr = htobe64(address);
			message.peek_v2.len = length;

			if (csp_cmp_peek_v2(node, timeout, &message) != CSP_ERR_NONE) {
				printf("No response\n");
				optparse_del(parser);
				return SLASH_EINVAL;
			}

			printf("Peek at address 0x%"PRIx64" len %u\n", address, length);
			csp_hex_dump(NULL, message.peek_v2.data, length);
		}
		break;
	}

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(peek, slash_csp_cmp_peek, "<address> <len>", "Peek");

static int slash_csp_cmp_poke(struct slash *slash)
{
	
	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
	unsigned int version = 1;

    optparse_t * parser = optparse_new("poke", "<addr> <data base16>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version, 1=32-bit <addr>>, 2=64-bit <addr> (default = 1)");

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
	uint64_t address = strtoull(slash->argv[argi], &endptr, 16);
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

	if (version < 1 || version > 2) {
		printf("Unsupported version: %d, only supports 1 (32-bit) and 2 (64-bit)\n", version);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	struct csp_cmp_message message;
	unsigned int data_str_len = strlen(slash->argv[argi]);

	if(data_str_len % 2 == 1){
		printf("Invalid length, needs to be whole bytes in hex\n");
		optparse_del(parser);
		return SLASH_EUSAGE;
	}

	switch (version) {
		case 1:
		{
			if (address > 0x00000000FFFFFFFFULL) {
				printf("Poke address out of 32-bit addressing range for version 1, try version 2.\n");
				optparse_del(parser);
				return SLASH_EINVAL;
			}

			if((data_str_len / 2) > CSP_CMP_POKE_MAX_LEN){
				printf("Max poke length %u\n", CSP_CMP_PEEK_MAX_LEN);
				optparse_del(parser);
				return SLASH_EUSAGE;
			}

			message.poke.addr = htobe32((address & 0x00000000FFFFFFFFULL));
			int outlen = base16_decode(slash->argv[argi], (uint8_t *) message.poke.data);
			message.poke.len = outlen;
			printf("Poke at address 0x%"PRIx32"\n", (uint32_t) (address & 0x00000000FFFFFFFFULL));
			csp_hex_dump(NULL, message.poke.data, outlen);

			if (csp_cmp_poke(node, timeout, &message) != CSP_ERR_NONE) {
				printf("No response\n");
				optparse_del(parser);
				return SLASH_EINVAL;
			}
		}
		break;
		case 2:
		{
			if((data_str_len / 2) > CSP_CMP_POKE_V2_MAX_LEN){
				printf("Max poke length %u\n", CSP_CMP_POKE_V2_MAX_LEN);
				optparse_del(parser);
				return SLASH_EUSAGE;
			}
			message.poke_v2.vaddr = htobe64(address);
			int outlen = base16_decode(slash->argv[argi], (uint8_t *) message.poke_v2.data);
			message.poke_v2.len = outlen;
			printf("Poke at address 0x%"PRIx64"\n", address);
			csp_hex_dump(NULL, message.poke_v2.data, outlen);

			if (csp_cmp_poke_v2(node, timeout, &message) != CSP_ERR_NONE) {
				printf("No response\n");
				optparse_del(parser);
				return SLASH_EINVAL;
			}
		}
		break;
	}

	printf("Poke ok\n");

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(poke, slash_csp_cmp_poke, "<address> <data>", "Poke");

const struct slash_command slash_cmd_time;
static int slash_csp_cmp_time(struct slash *slash)
{
	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
	unsigned int timestamp = 0;
	int sync = 0;
	int human_friendly = 0;

    optparse_t * parser = optparse_new_ex("time", "", slash_cmd_time.help);
    optparse_add_help(parser);
	optparse_add_set(parser, 's', "sync", 1, &sync, "sync time with CSH");
	optparse_add_set(parser, 'H', "human", 1, &human_friendly, "show the date/time info in human friendly manner");
	csh_add_node_option(parser, &node);
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

	csp_timestamp_t csp_localtime;
	csp_clock_get_time(&csp_localtime);

	if (sync) {
		message.clock.tv_sec = htobe32(csp_localtime.tv_sec);
		message.clock.tv_nsec = htobe32(csp_localtime.tv_nsec);
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
	int64_t local_time_ns = csp_localtime.tv_sec * (uint64_t) 1E9 + csp_localtime.tv_nsec;
	if(human_friendly) {
		char date_buffer[40];
		time_t local_time = csp_localtime.tv_sec;
		struct tm mt;
		localtime_r((const time_t *)&local_time, &mt);
		strftime(date_buffer, sizeof(date_buffer),"%Y-%m-%d %H:%M:%S %Z", &mt);
		printf("Local date/time:\n\t%s\n", date_buffer);
		struct tm in_utc;
		gmtime_r((const time_t *)&local_time, &in_utc);
		strftime(date_buffer, sizeof(date_buffer),"%Y-%m-%d %H:%M:%S %Z", &in_utc);
		printf("\t%s\n", date_buffer);

		time_t remote_time = message.clock.tv_sec;
		localtime_r((const time_t *)&remote_time, &mt);
		strftime(date_buffer, sizeof(date_buffer),"%Y-%m-%d %H:%M:%S %Z", &mt);
		printf("Date/time on Node %d:\n\t%s\n", node, date_buffer);
		gmtime_r((const time_t *)&remote_time, &in_utc);
		strftime(date_buffer, sizeof(date_buffer),"%Y-%m-%d %H:%M:%S %Z", &in_utc);
		printf("\t%s\n", date_buffer);		
	}

	printf("Remote time is %"PRIu32".%09"PRIu32" (diff %"PRIi64" ms)\n", message.clock.tv_sec, message.clock.tv_nsec, (remote_time_ns - local_time_ns) / 1000000);

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(time, slash_csp_cmp_time, NULL, "Get or synchronize timestamp");
