/*
 * param_commands_client.c
 *
 *  Created on: Sep 22, 2021
 *      Author: Mads
 */

#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>
#include <param/param.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <sys/types.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_client.h>
#include <param/param_queue.h>

extern param_queue_t param_queue;

typedef void (*param_transaction_callback_f)(csp_packet_t *response, int verbose, int version);
int param_transaction(csp_packet_t *packet, int host, int timeout, param_transaction_callback_f callback, int verbose, int version, void * context);

static void param_transaction_callback_add(csp_packet_t *response, int verbose, int version) {
    if (response->data[0] != PARAM_COMMAND_ADD_RESPONSE){
        return;
    }

	if (verbose) {
		if (be16toh(response->data16[1]) == UINT16_MAX) {
			printf("Adding command failed\n");
		} else {
			printf("Command added:\n");
		}
	}

	csp_buffer_free(response);
}

int param_command_push(param_queue_t *queue, int verbose, int server, char command_name[], int timeout) {
	if ((queue == NULL) || (queue->used == 0))
		return 0;

	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	if (queue->version == 2) {
		packet->data[0] = PARAM_COMMAND_ADD_REQUEST;
	} else {
		return -3;
	}

	packet->data[1] = 0;

	uint8_t name_length = (uint8_t) strlen(command_name);
	packet->data[2] = name_length;

	memcpy(&packet->data[3], command_name, name_length);

	memcpy(&packet->data[3+name_length], queue->buffer, queue->used);

	packet->length = queue->used + 3 + name_length;
	int result = param_transaction(packet, server, timeout, param_transaction_callback_add, verbose, queue->version, NULL);

	if (result < 0) {
		return -1;
	}

	if (verbose)
		param_queue_print(queue);

	return 0;
}

static void name_copy(char output[], char input[], int length) {
    for (int i = 0; i < length; i++) {
			output[i] = input[i];
    }
    output[length] = '\0';
}

static void param_transaction_callback_download(csp_packet_t *response, int verbose, int version) {
    if (response->data[0] != PARAM_COMMAND_SHOW_RESPONSE){
        return;
	}
    
	if (verbose) {
		int name_length = response->data[2];
		if (name_length > 13) {
			printf("Error: Requested command name not found.\n");
		} else {

			int size = response->length - (4 + name_length);
			char * data = (char *) &response->data[4 + name_length];
			memcpy(param_queue.buffer, data, size);
			param_queue.used = size;
			param_queue.type = response->data[3];
			param_queue.version = version;
			memcpy(param_queue.name, (char *) &response->data[4], name_length);
			param_queue.name[name_length] = '\0';

			param_queue_print(&param_queue);
		}
	}

	csp_buffer_free(response);
}

int param_command_download(int server, int verbose, char command_name[], int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_COMMAND_SHOW_REQUEST;
    packet->data[1] = 0;

    int name_length = strlen(command_name);

	packet->data[2] = name_length;

	memcpy(&packet->data[3], command_name, name_length);

	packet->length = 3+name_length;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_download, verbose, 2, NULL);

	if (result < 0) {
		return -1;
	}

	return 0;
}


static void param_transaction_callback_list(csp_packet_t *response, int verbose, int version) {
    if (response->data[0] != PARAM_COMMAND_LIST_RESPONSE){
        return;
    }
    
	if (verbose) {
		int num_cmds = be16toh(response->data16[1]);
		/* List the entries */
		if (num_cmds == 0) {
			printf("No saved commands\n");
		} else {
			printf("Received list of %u saved command names:\n", num_cmds);
			for (int i = 0; i < num_cmds; i++) {
				unsigned int idx = 4+i*14;
				char name[14];
				memcpy(name, &response->data[idx], 14);
				printf("%2u - %s\n", i+1, name);
			}
		}
	}

	csp_buffer_free(response);
}

int param_command_list(int server, int verbose, int timeout) {
	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_COMMAND_LIST_REQUEST;
	packet->data[1] = 0;

	packet->length = 2;

	int result = param_transaction(packet, server, timeout, param_transaction_callback_list, verbose, 2, NULL);

    if (result < 0) {
		return -1;
	}

	return 0;
}

static void param_transaction_callback_rm(csp_packet_t *response, int verbose, int version) {
    if (response->data[0] != PARAM_COMMAND_RM_RESPONSE){
        return;
	}
    
	if (verbose) {
		int rm_all_response = 0;
		char name[14] = {0};
		uint16_t response_data = be16toh(response->data16[1]);

		if (response->length == 14) {
			name_copy(name, (char *) &response->data[4], 9);
			char rmallcmds[] = "RMALLCMDS";
			size_t count = 0;
			for (size_t i = 0; i < strlen(rmallcmds); i++) {
				if (name[i] == rmallcmds[i]) {
					count++;
				} else {
					break;
				}
			}
			if (count == strlen(rmallcmds)) {
				rm_all_response = 1;
			}
		}

		if (rm_all_response) {
			printf("Deleted %u commands\n", response_data);
		} else {
			if (response_data < 14) {
				name_copy(name, (char *) &response->data[4], response_data);
				printf("Deleted command named %s\n", name);
			}
		}
	}

	csp_buffer_free(response);
}

int param_command_rm(int server, int verbose, char command_name[], int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_COMMAND_RM_REQUEST;
    packet->data[1] = 0;

    int name_length = strlen(command_name);

	packet->data[2] = name_length;

	memcpy(&packet->data[3], command_name, name_length);

	packet->length = 3+name_length;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_rm, verbose, 2, NULL);

	if (result < 0) {
		return -1;
	}

	return 0;
}

int param_command_rm_all(int server, int verbose, char command_name[], int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_COMMAND_RM_ALL_REQUEST;
    packet->data[1] = 0;

    int name_length = strlen(command_name);

	packet->data[2] = name_length;

	memcpy(&packet->data[3], command_name, name_length);

	packet->length = 3+name_length;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_rm, verbose, 2, NULL);

	if (result < 0) {
		return -1;
	}

	return 0;
}