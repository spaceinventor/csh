#include <stdio.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <apm/csh_api.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

static FILE * log_f = 0;
static char log_name[100] = {0};

static int stdbuf2_mon_slash(struct slash *slash) {

    unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
	char * log_name_tmp = 0;

    optparse_t * parser = optparse_new("stdbuf2", "");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_string(parser, 'f', "log", "STRING", &log_name_tmp, "Log file name");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
      optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (log_name_tmp) {
		if (strcmp(log_name_tmp, log_name)) {
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

			if ((strlen(log_name_tmp) > 0) && (log_name_tmp[0] == '?')) {
				log_name_tmp++; // skip '?'
				for (unsigned i = 1; i < 100; ++i) {
					snprintf(log_name, sizeof(log_name)-1, "%s_%s_%03u.log", 
						strlen(log_name_tmp) ? log_name_tmp : "csh", ts, i);
					log_name[sizeof(log_name)-1] = 0;
					if (access(log_name, F_OK) == -1) {
						break;
					}
				}
			} else {
				strncpy(log_name, log_name_tmp, sizeof(log_name) - 1); 
				log_name [sizeof(log_name) - 1] = 0;
			}

			log_f = fopen(log_name, access(log_name, F_OK) ? "w" : "a");		
			fprintf(log_f, "\n\n --- CSH log start %s ------------\n", ts);
			printf("Logging to %s\n", log_name);
		}
	}

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, 15, 0, CSP_O_CRC32);
  if (conn == NULL) {
    optparse_del(parser);
    return SLASH_ENOMEM;
  }

    csp_packet_t * packet = csp_buffer_get(1);
    if (packet == NULL) {
        csp_close(conn);
        optparse_del(parser);
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

  optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(stdbuf2, stdbuf2_mon_slash, NULL, "Monitor stdbuf");
