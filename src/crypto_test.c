/*
 * param_sniffer.c
 *
 *  Created on: Aug 12, 2020
 *      Author: joris
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include <param/param.h>
#include <param/param_list.h>

#include <csp/csp.h>
#include <csp/arch/csp_time.h>

/* Using un-exported header file.
 * This is allowed since we are still in libcsp */
#include <csp/arch/csp_thread.h>

#include "tweetnacl.h"
#include "crypto_test_param.h"

/** Example defines */
#define CSP_DECRYPTOR_PORT  20   // Address of local CSP node
#define TEST_TIMEOUT 1000

#define CRYPTO_REMOTE_KEY_COUNT 8

#define DIRECTION_UPLINK    1
#define DIRECTION_DOWNLINK  0


// Future Params for server
static uint8_t _crypto_key_public[crypto_box_PUBLICKEYBYTES];
static uint8_t _crypto_key_secret[crypto_box_SECRETKEYBYTES];
static uint8_t _crypto_key_remote[crypto_box_PUBLICKEYBYTES];
static uint8_t _crypto_beforenm[crypto_box_BEFORENMBYTES];

uint64_t _crypto_remote_counter;
uint64_t _crypto_fail_auth_count;
uint64_t _crypto_fail_nonce_count;


// ------------------------
// Temporary Debug functions
// ------------------------
void crypto_test_print_hex(char * text, unsigned char * data, int length) {
    printf("    %-25s: ", text);
    for(int i = 0; i < length; i++) {
        printf("%02hhX, ", data[i]);
    }
    printf("\n");
}
#define CRYPTO_TEST_PRINT_HEX(EXP) crypto_test_print_hex(#EXP, EXP, sizeof(EXP))

// ------------------------
// Crypto Helper Functions
// ------------------------
void randombytes(unsigned char * a, unsigned long long c) {
    // Note: Pseudo random since we are not initializing random!
    time_t t;
    srand((unsigned) time(&t) + rand());
    while(c > 0) {
        *a = rand() & 0xFF;
        a++;
        c--;
    }
}

uint8_t crypto_compare_pkey(uint8_t * own_key, uint8_t * other_key) {
    int result = memcmp(own_key, other_key, crypto_box_PUBLICKEYBYTES);
    if(result > 0) {
        return 1;
    }
    else {
        return 0;
    }
}

void crypto_test_make_nonce(uint8_t * nonce, uint64_t counter)  {
    memset(nonce, 0, crypto_box_NONCEBYTES);

    // Nonce format: first 8 bytes is 64bit counter.
    uint8_t *p = (uint8_t *)&counter;
    for(int i = 0; i < 8; i++) {
        nonce[i] = p[i];
    }
}

uint64_t crypto_test_get_counter(uint8_t * nonce) {
    uint64_t counter = 0;

    // Nonce format: first 8 bytes is 64bit counter.
    uint8_t *p = (uint8_t *)&counter;
    for(int i = 0; i < 8; i++) {
        p[i] = nonce[i];
    }
    return counter;
}

//#define crypto_secretbox_xsalsa20poly1305_tweet_ZEROBYTES 32
//#define crypto_secretbox_xsalsa20poly1305_tweet_BOXZEROBYTES 16
/*
There is a 32-octet padding requirement on the plaintext buffer that you pass to crypto_box.
Internally, the NaCl implementation uses this space to avoid having to allocate memory or
use static memory that might involve a cache hit (see Bernstein's paper on cache timing
side-channel attacks for the juicy details).

Similarly, the crypto_box_open call requires 16 octets of zero padding before the start
of the actual ciphertext. This is used in a similar fashion. These padding octets are not
part of either the plaintext or the ciphertext, so if you are sending ciphertext across the
network, don't forget to remove them!
*/

csp_packet_t * crypto_test_csp_encrypt_packet(uint8_t * data, unsigned int size, uint8_t * key_beforem, uint64_t * counter, uint8_t nonce_counter_parity) {

    int result = 1;

    csp_packet_t * packet = NULL;
    csp_packet_t * buffer = NULL;

    printf("    crypto_test_csp_encrypted_packet %d - counter=%lld\n", size, *counter);

    // Allocate additional buffer to pre-pad input data
    buffer = csp_buffer_get(size + crypto_secretbox_ZEROBYTES);
    if (buffer == NULL)
        return NULL;

    // Copy input data to temporary buffer prepended with zeros
    memset(buffer->data, 0, crypto_secretbox_ZEROBYTES);
    memcpy(buffer->data + crypto_secretbox_ZEROBYTES, data, size);

    // HACKZORS
    *counter = (*counter & 0xFFFFFFFE) + 1 + nonce_counter_parity;
    printf("    new counter=%lld\n", *counter);

    unsigned char nonce[crypto_box_NONCEBYTES]; //24
    crypto_test_make_nonce(nonce, *counter);

    /* Prepare data */
    packet = csp_buffer_get(size + crypto_secretbox_ZEROBYTES);
    if (packet == NULL)
        return NULL;

    result = crypto_box_afternm(packet->data - 16 + 4, buffer->data, size + crypto_secretbox_ZEROBYTES, nonce, key_beforem);

    csp_buffer_free(buffer);

    if (result != 0) {
    	csp_buffer_free(packet);
        return NULL;
    }

    // Use cyphertext 0-padding for nonce to avoid additonal memcpy's
    packet->length = size + 16 + 4;
    memcpy(packet->data, nonce, 4);

    printf("    Sending [%s]\n", data);
    CRYPTO_TEST_PRINT_HEX(nonce);
    crypto_test_print_hex("buffer->data", buffer->data, size + crypto_secretbox_ZEROBYTES);
    crypto_test_print_hex("packet->data", packet->data, packet->length);

    return packet;
}

int crypto_test_csp_decrypt_packet(csp_packet_t * packet, uint8_t * key_beforem, uint64_t * counter, uint8_t nonce_counter_parity) {

    // Allocate an extra buffer for de decrypted data
    csp_packet_t * buffer = NULL;

    buffer = csp_buffer_get(packet->length - crypto_box_BOXZEROBYTES + crypto_box_ZEROBYTES);
    if (buffer == NULL)
        return -1;

    // Extract NONCE from packet, then set padding to zero as required
    unsigned char nonce[crypto_box_NONCEBYTES];
    memset(nonce, 0, crypto_box_NONCEBYTES);
    memcpy(nonce, packet->data, 4);
    memset(packet->data - 16 + 4, 0, crypto_box_BOXZEROBYTES);

    CRYPTO_TEST_PRINT_HEX(nonce);
    crypto_test_print_hex("packet->data", packet->data - 16 + 4, packet->length + 16 - 4);

    if(crypto_box_open_afternm(buffer->data, packet->data - 16 + 4, packet->length + 16 - 4, nonce, key_beforem) != 0) {
        return -1;
    }

    // Message successfully decrypted, only then check if nonce was valid
    uint64_t nonce_counter = crypto_test_get_counter(nonce);
    printf("Counter: %lld - Nonce: %lld\n", *counter, nonce_counter);
    if(nonce_counter <= *counter) {
        printf("Nonce check failed\n");
        csp_buffer_free(buffer);
        return -1;
    }

    // Update counter with received value so that next sent value is higher
    *counter = nonce_counter;

    crypto_test_print_hex("buffer->data", buffer->data, packet->length);
    printf("    Decrypted packet: [%s]\n", buffer->data + crypto_secretbox_ZEROBYTES);

    csp_buffer_free(buffer);

    return 0;
}

int crypto_test_echo(uint8_t node, uint8_t * data, unsigned int size) {
    int result;
    uint32_t start, time, status = 0;

    uint8_t nonce_counter_parity = crypto_compare_pkey(_crypto_key_remote, _crypto_key_public);
    printf("    crypto_test_echo %d bytes, parity=%d\n", size, nonce_counter_parity);

    // Counter
    start = csp_get_ms();

    // Open connection
    csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, node, CSP_DECRYPTOR_PORT, TEST_TIMEOUT, CSP_O_NONE);
    if (conn == NULL)
        return -1;

    csp_packet_t * packet = NULL;
    packet = crypto_test_csp_encrypt_packet(data, size, _crypto_beforenm, &_crypto_remote_counter, nonce_counter_parity);
    if (packet == NULL)
        goto out;

    // Try to send frame
    if (!csp_send(conn, packet, 0))
        goto out;

    // Read incoming frame
    packet = csp_read(conn, TEST_TIMEOUT);
    if (packet == NULL)
        goto out;

    printf("    Received\n");
    result = crypto_test_csp_decrypt_packet(packet, _crypto_beforenm, &_crypto_remote_counter, nonce_counter_parity);
    if(result != 0)
        goto out;

    status = 1;

out:
    /* Clean up */
    if (packet != NULL)
        csp_buffer_free(packet);

    csp_close(conn);

    /* We have a reply */
    time = (csp_get_ms() - start);

    if (status) {
        printf("    Great Success\n");
        return time;
    } else {
        printf("    Failed\n");
        return -1;
    }

}

void crypto_test_packet_handler(csp_packet_t * packet) {
    int result;
    csp_packet_t * reply_packet = NULL;

    uint8_t nonce_counter_parity = crypto_compare_pkey(_crypto_key_remote, _crypto_key_public);
    printf("    --------\n    crypto_test_packet_handler %d bytes, parity=%d\n", packet->length, nonce_counter_parity);

    result = crypto_test_csp_decrypt_packet(packet, _crypto_beforenm, &_crypto_remote_counter, nonce_counter_parity);
    csp_buffer_free(packet);
    if(result != 0)
        return;

    char reply[] = "SUCCESS";
    reply_packet = crypto_test_csp_encrypt_packet((uint8_t*)reply, sizeof(reply), _crypto_beforenm, &_crypto_remote_counter, nonce_counter_parity);
    if(reply_packet == NULL) {
    	return;
    }

    if (csp_sendto_reply(packet, reply_packet, CSP_O_SAME, 0) != CSP_ERR_NONE) {
    	csp_buffer_free(reply_packet);
    }
}

void crypto_test_key_refresh(void) {

	/* Read keys from vmem/config file */
	param_get_data(&crypto_key_public, _crypto_key_public, crypto_box_PUBLICKEYBYTES);
	param_get_data(&crypto_key_secret, _crypto_key_secret, crypto_box_SECRETKEYBYTES);
	param_get_data(&crypto_key_remote, _crypto_key_remote, crypto_box_PUBLICKEYBYTES);

	/* Pre compute stuff */
    crypto_box_beforenm(_crypto_beforenm, _crypto_key_remote, _crypto_key_secret);

}

void crypto_test_generate_local_key(void) {
    int result;

	result = crypto_box_keypair(_crypto_key_public, _crypto_key_secret);
    if(result != 0) {
    	printf("FAIL: crypto box keypair\n");
    	return;
	}

    param_set_data(&crypto_key_public, _crypto_key_public, crypto_box_PUBLICKEYBYTES);
    param_set_data(&crypto_key_secret, _crypto_key_secret, crypto_box_SECRETKEYBYTES);

    param_print(&crypto_key_public, 0, 0, 0, 2);
    param_print(&crypto_key_secret, 0, 0, 0, 2);

}

void crypto_test_init(void) {

	crypto_test_key_refresh();

    /* Server */
    csp_socket_t *sock_crypto = csp_socket(CSP_SO_NONE);
    csp_socket_set_callback(sock_crypto, crypto_test_packet_handler);
    csp_bind(sock_crypto, CSP_DECRYPTOR_PORT);

}
