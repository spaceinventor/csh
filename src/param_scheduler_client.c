/*
 * param_scheduler_client.c
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>
#include <time.h>
#include <param/param.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <sys/types.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_client.h>
#include <param/param_queue.h>

typedef void (*param_transaction_callback_f)(csp_packet_t *response, int verbose, int version);
int param_transaction(csp_packet_t *packet, int host, int timeout, param_transaction_callback_f callback, int verbose, int version, void * context);

static void param_transaction_callback_add(csp_packet_t *response, int verbose, int version) {
    if (response->data[0] != PARAM_SCHEDULE_ADD_RESPONSE){
        return;
    }

	if (verbose) {
		if (be16toh(response->data16[1]) == UINT16_MAX) {
			printf("\033[0;31mScheduling queue failed\033[0m\n");
		} else {
			printf("Queue scheduled with id: %u\n", be16toh(response->data16[1]));
		}
	}

	csp_buffer_free(response);
}

int param_schedule_push(param_queue_t *queue, int verbose, int server, uint16_t host, uint32_t time, uint32_t latency_buffer, int timeout) {
	if ((queue == NULL) || (queue->used == 0))
		return 0;

	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	if (queue->version == 2) {
		packet->data[0] = PARAM_SCHEDULE_PUSH;
	} else {
		return -3;
	}

	packet->data[1] = 0;
    packet->data16[1] = htobe16(host);
    packet->data32[1] = htobe32(time);
	packet->data32[2] = htobe32(latency_buffer);

	memcpy(&packet->data[12], queue->buffer, queue->used);

	packet->length = queue->used + 12;
	int result = param_transaction(packet, server, timeout, param_transaction_callback_add, verbose, queue->version, NULL);

	if (result < 0) {
		return -1;
	}

	if (verbose)
		param_queue_print(queue);

	return 0;
}

#if 0
int param_schedule_pull(param_queue_t *queue, int verbose, int server, uint16_t host, uint32_t time, int timeout) {

	if ((queue == NULL) || (queue->used == 0))
		return 0;

	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	if (queue->version == 2) {
		packet->data[0] = PARAM_SCHEDULE_PULL;
	} else {
		return -3;
	}

	packet->data[1] = 0;
    packet->data16[1] = htobe16(host);
    packet->data32[1] = htobe32(time);

	memcpy(&packet->data[8], queue->buffer, queue->used);

	packet->length = queue->used + 8;
	int result = param_transaction(packet, server, timeout, param_transaction_callback_add, verbose, queue->version, NULL);

	if (result < 0) {
		return -1;
	}

	if (verbose)
		param_queue_print(queue);

	return 0;
}
#endif

static void param_transaction_callback_show(csp_packet_t *response, int verbose, int version) {
	//csp_hex_dump("pull response", response->data, response->length);
    if (response->data[0] != PARAM_SCHEDULE_SHOW_RESPONSE){
        return;
	}
    
	if (verbose) {
		int id = be16toh(response->data16[1]);
		if (id == UINT16_MAX) {
			printf("Requested schedule id not found.\n");
		} else {
			param_queue_t queue;
			param_queue_init(&queue, &response->data[10], response->length - 10, response->length - 10, response->data[8], version);
			int time = be32toh(response->data32[1]);
			//queue.last_node = response->id.src;

			time_t timestamp = (long) time;
			char timestr[32] ={0};
			struct tm * sch_datetime = gmtime(&timestamp);
			strftime(timestr, sizeof(timestr)-1, "%F T%TZ%z", sch_datetime);

			/* Show the requested queue */
			printf("Showing queue id %u ", id);
			if (response->data[9] == 0) { // if not completed
				printf("scheduled at server time: %u (%s)\n", time, timestr);
			} else if (response->data[9] == 0x55) {
				printf("\033[0;32mcompleted at server time: %u (%s)\033[0m\n", time, timestr);
			} else if (response->data[9] == 0xAA) {
				printf("\033[0;31mexceeded latency buffer at server time: %u (%s)\033[0m\n", time, timestr);
			}

			param_queue_print(&queue);

		}
	}

	csp_buffer_free(response);
}

int param_show_schedule(int server, int verbose, uint16_t id, int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_SHOW_REQUEST;
    packet->data[1] = 0;

    packet->data16[1] = htobe16(id);

	packet->length = 4;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_show, verbose, 2, NULL);

	if (result < 0) {
		return -1;
	}

	return 0;
}

static void param_transaction_callback_list(csp_packet_t *response, int verbose, int version) {
	//csp_hex_dump("pull response", response->data, response->length);
    if (response->data[0] != PARAM_SCHEDULE_LIST_RESPONSE){
        return;
    }
    
	if (verbose) {
		int num_scheduled = be16toh(response->data16[1]);
		/* List the entries */
		if (num_scheduled == 0) {
			printf("No scheduled queues\n");
		} else {
			printf("Received list of %u queues:\n", num_scheduled);
			for (int i = 0; i < num_scheduled; i++) {
				unsigned int idx = 4+i*(4+2+2);
				unsigned int sch_time = be32toh(response->data32[idx/4]);

				time_t timestamp = (long) sch_time;
				char timestr[32] ={0};
				struct tm * sch_datetime = gmtime(&timestamp);
				strftime(timestr, sizeof(timestr)-1, "%F T%TZ%z", sch_datetime);

				if (response->data[idx+7] == PARAM_QUEUE_TYPE_SET) {
					printf("[SET] Queue id %u ", be16toh(response->data16[idx/2+2]));
				} /*else {
					printf("[GET] Queue id %u ", be16toh(response->data16[idx/2+2]));
				}*/
				if (response->data[idx+6] == 0x55) {
					printf("\033[0;32mcompleted at server time: %s (%u)\033[0m\n", timestr, sch_time);
				} else if (response->data[idx+6] == 0) {
					printf("scheduled at server time: %s (%u)\n", timestr, sch_time);
				} else {
					printf("\033[0;31mexceeded latency buffer at server time: %s (%u)\033[0m\n", timestr, sch_time);
				}
			}
		}
	}

	csp_buffer_free(response);
}

int param_list_schedule(int server, int verbose, int timeout) {
    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_LIST_REQUEST;
    packet->data[1] = 0;

	packet->length = 2;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_list, verbose, 2, NULL);

    if (result < 0) {
		return -1;
	}

	return 0;
}

static void param_transaction_callback_rm(csp_packet_t *response, int verbose, int version) {
	//csp_hex_dump("pull response", response->data, response->length);
    if (response->data[0] != PARAM_SCHEDULE_RM_RESPONSE){
        return;
	}
    
	if (verbose) {
		if ((be16toh(response->data16[1]) == UINT16_MAX) && (response->length == 6) ) {
			//RM ALL RESPONSE
			printf("All %u scheduled parameter queues removed.\n", be16toh(response->data16[2]));
		} else {
			//RM SINGLE RESPONSE
			printf("Schedule id %u removed.\n", be16toh(response->data16[1]));
		}
	}
    
	csp_buffer_free(response);
}

int param_rm_schedule(int server, int verbose, uint16_t id, int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_RM_REQUEST;

    packet->data[1] = 0;

    packet->data16[1] = htobe16(id);
	
	packet->length = 4;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_rm, verbose, 2, NULL);

	if (result < 0) {
		return -1;
	}

	return 0;
}

int param_rm_all_schedule(int server, int verbose, int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_RM_ALL_REQUEST;

    packet->data[1] = 0;

    packet->data16[1] = htobe16(UINT16_MAX);

	packet->length = 4;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_rm, verbose, 2, NULL);

	if (result < 0) {
		return -1;
	}

	return 0;
}

static void param_transaction_callback_reset(csp_packet_t *response, int verbose, int version) {
	if (response->data[0] != PARAM_SCHEDULE_RESET_RESPONSE){
        return;
	}
    
	if (verbose) {
		printf("Scheduler server reset: last id = %u\n", be16toh(response->data16[1]));
	}
    
	csp_buffer_free(response);
}

int param_reset_scheduler(int server, uint16_t last_id, int verbose, int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_RESET_REQUEST;

    packet->data[1] = 0;

	packet->data16[1] = htobe16(last_id);

	packet->length = 4;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_reset, verbose, 2, NULL);

	if (result < 0) {
		return -1;
	}

	return 0;
}

static void param_transaction_callback_schedule_cmd(csp_packet_t *response, int verbose, int version) {
    if (response->data[0] != PARAM_SCHEDULE_ADD_RESPONSE){
        return;
    }

	if (verbose) {
		if (be16toh(response->data16[1]) == UINT16_MAX) {
			printf("Scheduling command failed:\n");
		} else {
			printf("Command scheduled with id %u: ", be16toh(response->data16[1]));
		}
	}

	csp_buffer_free(response);
}

int param_schedule_command(int verbose, int server, char command_name[], uint16_t host, uint32_t time, uint32_t latency_buffer, int timeout) {
	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_COMMAND_REQUEST;

	packet->data[1] = 0;
    packet->data16[1] = htobe16(host);
    packet->data32[1] = htobe32(time);
	packet->data32[2] = htobe32(latency_buffer);

	int name_length = strlen(command_name);
	memcpy(&packet->data[12], command_name, name_length);

	packet->length = 12+name_length;
	if (param_transaction(packet, server, timeout, param_transaction_callback_schedule_cmd, verbose, 2, NULL) < 0)
		return -1;

	if (verbose)
		printf("%s\n", command_name);

	return 0;
}
