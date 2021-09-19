/*
 * vmem_client_slash.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */


#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <csp/csp.h>
#include <sys/types.h>

#include <slash/slash.h>

#include "tweetnacl.h"
#include "crypto.h"

/** Example defines */
#define CSP_DECRYPTOR_PORT  20   // Address of local CSP node
#define TEST_TIMEOUT 1000

int crypto_test_echo(uint8_t node, uint8_t * data, unsigned int size) {

    csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, node, CSP_DECRYPTOR_PORT, TEST_TIMEOUT, CSP_O_NONE);
    if (conn == NULL)
        return -1;

    csp_packet_t * packet = csp_buffer_get(size + crypto_secretbox_BOXZEROBYTES);
	if (packet == NULL) {
		csp_close(conn);
		return -1;
	}

	/* Zerofill data (in temporary buffer) */
    uint8_t * msg_buf = malloc(size + crypto_secretbox_ZEROBYTES);
    if (msg_buf == NULL) {
    	csp_close(conn);
    	csp_buffer_free(packet);
    }

    uint8_t * msg_ptr = msg_buf + crypto_secretbox_ZEROBYTES;
    int msg_len;
    memcpy(msg_ptr, data, size);

    /* Encrypt */
	packet->length = crypto_encrypt_with_zeromargin(msg_ptr, size + crypto_secretbox_ZEROBYTES, packet->data);

    csp_hex_dump("packet", packet->data, packet->length);

    // Try to send frame
    if (!csp_send(conn, packet, 0)) {
    	printf("Send failed\n");
        csp_buffer_free(packet);
        csp_close(conn);
    	free(msg_buf);
        return -1;
    }


    // Read incoming frame
    packet = csp_read(conn, TEST_TIMEOUT);
    if (packet == NULL) {
    	printf("timeout\n");
        csp_close(conn);
        free(msg_buf);
        return -1;
    }

	int length = crypto_decrypt_with_zeromargin(packet->data, packet->length, msg_ptr);
	if (length < 0) {
		printf("Decryption error\n");
		csp_buffer_free(packet);
		free(msg_buf);
		return -1;
	} else {
		msg_len = length;
	}

	csp_hex_dump("msg", msg_buf, msg_len);

	free(msg_buf);
    csp_buffer_free(packet);
    csp_close(conn);

    return 0;

}

void crypto_test_packet_handler(csp_packet_t * packet) {

    uint8_t * msg_buf = malloc(packet->length + crypto_secretbox_ZEROBYTES);
    uint8_t * msg_ptr = msg_buf + crypto_secretbox_ZEROBYTES;
    int msg_len;

    int length = crypto_decrypt_with_zeromargin(packet->data, packet->length, msg_ptr);
	if (length < 0) {
		csp_buffer_free(packet);
		free(msg_buf);
		printf("Decryption error\n");
		return;
	} else {
		msg_len = length;
	}

	csp_hex_dump("msg", msg_buf, msg_len);

	csp_packet_t * new_packet = csp_buffer_get(msg_len);
	if (new_packet == NULL) {
		csp_buffer_free(packet);
		free(msg_buf);
		return;
	}

	/* Encrypt */
	new_packet->length = crypto_encrypt_with_zeromargin(msg_ptr, msg_len, new_packet->data);

	csp_hex_dump("new packet", new_packet->data, new_packet->length);

    if (csp_sendto_reply(packet, new_packet, CSP_O_SAME, 0) != CSP_ERR_NONE) {
    	csp_buffer_free(new_packet);
    }

    csp_buffer_free(packet);
    free(msg_buf);
}

static int crypto_startrx_cmd(struct slash *slash) {

    /* Server */
    csp_socket_t *sock_crypto = csp_socket(CSP_SO_NONE);
    csp_socket_set_callback(sock_crypto, crypto_test_packet_handler);
    csp_bind(sock_crypto, CSP_DECRYPTOR_PORT);

    return SLASH_SUCCESS;
}

slash_command_sub(crypto, startrx, crypto_startrx_cmd, NULL, NULL);

static int crypto_send_cmd(struct slash *slash)
{

    int node = csp_get_address();
    int timeout = 2000;
    char * endptr;

    if (slash->argc >= 2) {
        node = strtoul(slash->argv[1], &endptr, 10);
        if (*endptr != '\0')
            return SLASH_EUSAGE;
    }

    if (slash->argc >= 3) {
        timeout = strtoul(slash->argv[2], &endptr, 10);
        if (*endptr != '\0')
            return SLASH_EUSAGE;
    }

    printf("Sending encrypted packed to %u timeout %u\n", node, timeout);

    char test[] = "Hello this is a test message!!";
    crypto_test_echo(node, (uint8_t*)test, sizeof(test));

    return SLASH_SUCCESS;
}

slash_command_sub(crypto, send, crypto_send_cmd, "<node> <timeout>", NULL);

static int crypto_generate_cmd(struct slash *slash)
{
	crypto_generate_local_key();
    return SLASH_SUCCESS;
}

slash_command_sub(crypto, generate, crypto_generate_cmd, NULL, NULL);


