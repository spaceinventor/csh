#include <stdio.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <vmem/vmem_server.h>
#include <param/param_queue.h>
#include <param/param_client.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <apm/csh_api.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

static vmem_list_t stdbuf_get_base(int node, int timeout) {
    vmem_list_t ret = {};

    csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_CRC32);
    if (conn == NULL)
        return ret;

    csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
    vmem_request_t * request = (void *) packet->data;
    request->version = 1;
    request->type = VMEM_SERVER_LIST;
    packet->length = sizeof(vmem_request_t);

    csp_send(conn, packet);

    /* Wait for response */
    packet = csp_read(conn, timeout);
    if (packet == NULL) {
        printf("No response\n");
        csp_close(conn);
        return ret;
    }

    for (vmem_list_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
        //printf(" %u: %-5.5s 0x%08X - %u typ %u\r\n", vmem->vmem_id, vmem->name, (unsigned int) be32toh(vmem->vaddr), (unsigned int) be32toh(vmem->size), vmem->type);
        if (strncmp(vmem->name, "stdbu", 5) == 0) {
            ret.vmem_id = vmem->vmem_id;
            ret.type = vmem->type;
            memcpy(ret.name, vmem->name, 5);
            ret.vaddr = be32toh(vmem->vaddr);
            ret.size = be32toh(vmem->size);
        }
    }

    csp_buffer_free(packet);
    csp_close(conn);

    return ret;
}

static int stdbuf_get(uint16_t node, uint32_t base, int from, int to, int timeout) {

    int len = to - from;
    if (len > 200)
        len = 200;

    struct csp_cmp_message message;
    message.peek.addr = htobe32(base + from);
    message.peek.len = len;

    if (csp_cmp_peek(node, timeout, &message) != CSP_ERR_NONE) {
        return 0;
    }

    int ignore __attribute__((unused)) = write(fileno(stdout), message.peek.data, len);

    return len;

}

static int stdbuf_v1(struct slash *slash, unsigned int node, unsigned int timeout, unsigned int paramversion) {

    /* Pull buffer */
    char pull_buf[25];
    param_queue_t pull_q = {
        .buffer = pull_buf,
        .buffer_size = 50,
        .type = PARAM_QUEUE_TYPE_GET,
        .used = 0,
        .version = paramversion,
    };

    vmem_list_t vmem = stdbuf_get_base(node, 1000);

    if (vmem.size == 0 || vmem.vaddr == 0) {
        printf("Could not find stdbuffer on node %u\n", node);
        return SLASH_EINVAL;
    }

    param_t * stdbuf_in = param_list_find_id(node, 28);
    if (stdbuf_in == NULL) {
        stdbuf_in = param_list_create_remote(28, node, PARAM_TYPE_UINT16, PM_DEBUG, 0, "stdbuf_in", NULL, NULL, -1);
        param_list_add(stdbuf_in);
    }

    param_t * stdbuf_out = param_list_find_id(node, 29);
    if (stdbuf_out == NULL) {
        stdbuf_out = param_list_create_remote(29, node, PARAM_TYPE_UINT16, PM_DEBUG, 0, "stdbuf_out", NULL, NULL, -1);
        param_list_add(stdbuf_out);
    }

    param_queue_add(&pull_q, stdbuf_in, 0, NULL);
    param_queue_add(&pull_q, stdbuf_out, 0, NULL);

    printf("Monitoring stdbuf on node %u, base %x, size %u\n", node, vmem.vaddr, vmem.size);

    while(1) {

        param_pull_queue(&pull_q, CSP_PRIO_HIGH, 0, node, 100);
        int in = param_get_uint16(stdbuf_in);
        int out = param_get_uint16(stdbuf_out);

        int got = 0;
        if (out < in) {
            //printf("Get from %u to %u\n", out, in);
            got = stdbuf_get(node, vmem.vaddr, out, in, 100);
        } else if (out > in) {
            //printf("Get from %u to %u\n", out, vmem.size);
            got = stdbuf_get(node, vmem.vaddr, out, vmem.size, 100);
        }

        uint16_t out_push = out + got;
        out_push %= vmem.size;
        param_push_single(stdbuf_out, 0, CSP_PRIO_NORM, &out_push, 0, node, 100, paramversion, false);

        if (got > 0) {
            continue;
        }

        /* Delay (press enter to exit) */
        if (slash_wait_interruptible(slash, 100) != 0) {
            break;
        }

    };

    return SLASH_SUCCESS;
}

static FILE * log_f = 0;
static char log_name[100] = {0};

static int stdbuf_v2(unsigned int node, unsigned int timeout, char * logfile) {

    if (logfile) {
        if (strcmp(logfile, log_name)) {
            if (log_f) {
                fclose(log_f);
                log_f = 0;
            }
        }
        if (!log_f) {
            time_t timer = time(NULL);
            struct tm * tm_info = localtime(&timer);
            char ts[30];
            strftime(ts, 30, "%Y%m%d", tm_info);	

            if ((strlen(logfile) > 0) && (logfile[0] == '?')) {
                logfile++; // skip '?'
                for (unsigned i = 1; i < 100; ++i) {
                    snprintf(log_name, sizeof(log_name)-1, "%s_%s_%03u.log", 
                        strlen(logfile) ? logfile : "csh", ts, i);
                    log_name[sizeof(log_name)-1] = 0;
                    if (access(log_name, F_OK) == -1) {
                        break;
                    }
                }
            } else {
                strncpy(log_name, logfile, sizeof(log_name) - 1); 
                log_name [sizeof(log_name) - 1] = 0;
            }

            log_f = fopen(log_name, access(log_name, F_OK) ? "w" : "a");		
            fprintf(log_f, "\n\n --- CSH log start %s ------------\n", ts);
            printf("Logging to %s\n", log_name);
        }
    }

    csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, 15, 0, CSP_O_CRC32);
    if (conn == NULL) {
        return SLASH_ENOMEM;
    }

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
        if (log_f) {
            int i = 1;
            while (i < packet->length) {
                if (isprint(packet->data[i])) {
                    fprintf(log_f, "%c", packet->data[i]);
                    i++;
                } else if ((packet->data[i] == '\r') || (packet->data[i] == '\n')) {
                    fprintf(log_f, "\n");
                    while ((i < packet->length) && ((packet->data[i] == '\r') || (packet->data[i] == '\n'))) {
                        i++;
                    }
                } else  {
                    // Other control character
                    fprintf(log_f, "0x%02x", packet->data[i]);
                    i++;
                }
            }
        }

        int again = packet->data[0];
        csp_buffer_free(packet);

        if (again == 0) {
            break;
        }
    }

    csp_close(conn);

    return SLASH_SUCCESS;
}

static int stdbuf_slash(struct slash *slash) {

    unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int version = 2;
    unsigned int paramversion = 2;
    char * logfile = NULL;

    optparse_t * parser = optparse_new("stdbuf2", "");
    optparse_add_help(parser);
    csh_add_node_option(parser, &node);
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 2)");
    optparse_add_unsigned(parser, 'p', "paramversion", "NUM", 0, &paramversion, "paramversion (default = 2)");
    optparse_add_string(parser, 'f', "logfile", "STRING", &logfile, "Log file name");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
      optparse_del(parser);
        return SLASH_EINVAL;
    }

    int result;
    if (version == 2) {
        result = stdbuf_v2(node, timeout, logfile);
    } else if(version == 1) {
        result = stdbuf_v1(slash, node, timeout, paramversion);
    } else {
        result = SLASH_EUSAGE;
    }

    optparse_del(parser);
    return result;
}

slash_command(stdbuf, stdbuf_slash, NULL, "Monitor stdbuf");
slash_command(stdbuf2, stdbuf_slash, NULL, "Monitor stdbuf");
