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

#include <csp/csp.h>
#include <csp/arch/csp_time.h>

/* Using un-exported header file.
 * This is allowed since we are still in libcsp */
#include <csp/arch/csp_thread.h>

#include "tweetnacl.h"

/** Example defines */
#define CSP_DECRYPTOR_PORT  20   // Address of local CSP node
#define TEST_TIMEOUT 1000

#define CRYPTO_REMOTE_KEY_COUNT 8

#define DIRECTION_UPLINK    1
#define DIRECTION_DOWNLINK  0


// Future Params for server
uint8_t _crypto_key_public[crypto_box_PUBLICKEYBYTES];
uint8_t _crypto_key_secret[crypto_box_SECRETKEYBYTES];

uint8_t _crypto_remote_key[crypto_box_PUBLICKEYBYTES];
uint64_t _crypto_remote_counter;
uint64_t _crypto_fail_auth_count;
uint64_t _crypto_fail_nonce_count;

uint8_t crypto_remote_beforem[crypto_box_BEFORENMBYTES];

// Test for client
unsigned char _crypto_key_test_public[crypto_box_PUBLICKEYBYTES] = {0};
unsigned char _crypto_key_test_secret[crypto_box_SECRETKEYBYTES] = {0};
unsigned char _crypto_key_test_beforem[crypto_box_BEFORENMBYTES] = {0};
uint64_t _crypto_test_counter;


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

/*
void * debug_csp_buffer_get(size_t size) {
    printf("csp_buffer_get(%d)\n", size);
    return csp_buffer_get(size);
}
#define csp_buffer_get debug_csp_buffer_get
*/

void crypto_test_generate_local_key() {
    int result;

    printf("Run crypto_test_generate_local_key\n");

    if(_crypto_key_public[0] == 0) {
        printf("    Run crypto_box_keypair\n");
        result = crypto_box_keypair(_crypto_key_public, _crypto_key_secret);
        if(result != 0) {
            printf("    ERROR\n");
        }
    }
    else {
        printf("    Using hardcoded key");
    }
    CRYPTO_TEST_PRINT_HEX(_crypto_key_public);
    CRYPTO_TEST_PRINT_HEX(_crypto_key_secret);
}

void crypto_test_generate_keys() {
    int result;

    crypto_test_generate_local_key();

    if(_crypto_key_test_public[0] == 0) {
        printf("    Run crypto_box_keypair\n");
        result = crypto_box_keypair(_crypto_key_test_public, _crypto_key_test_secret);
        if(result != 0) {
            printf("    ERROR\n");
        }
    }
    else {
        printf("    Using hardcoded key");
    }
    CRYPTO_TEST_PRINT_HEX(_crypto_key_test_public);
    CRYPTO_TEST_PRINT_HEX(_crypto_key_test_secret);

    printf("    Run crypto_box_beforenm\n");
    result = crypto_box_beforenm(_crypto_key_test_beforem, _crypto_key_public, _crypto_key_test_secret);
    if(result != 0) {
        printf("    ERROR\n");
    }

    // Assign Test key to Slot 0
    memcpy(_crypto_remote_key, _crypto_key_test_public, sizeof(_crypto_key_test_public));
}

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
        goto out;

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
        goto out;

    result = crypto_box_afternm(packet->data, buffer->data, size + crypto_secretbox_ZEROBYTES, nonce, key_beforem);
    if (result != 0)
        goto out;

    // Use cyphertext 0-padding for nonce to avoid additonal memcpy's
    packet->length = size + crypto_secretbox_ZEROBYTES;
    memcpy(packet->data, nonce, crypto_secretbox_BOXZEROBYTES);

    printf("    Sending [%s]\n", data);
    CRYPTO_TEST_PRINT_HEX(nonce);
    crypto_test_print_hex("buffer->data", buffer->data, size + crypto_secretbox_ZEROBYTES);
    crypto_test_print_hex("packet->data", packet->data, size + crypto_secretbox_BOXZEROBYTES + 16);

out:
    if (buffer != NULL)
        csp_buffer_free(buffer);

    if (result != 0)
        csp_buffer_free(packet);

    return packet;
}

int crypto_test_csp_decrypt_packet(csp_packet_t * packet, uint8_t * key_beforem, uint64_t * counter, uint8_t nonce_counter_parity) {
    int result = 1;

    // Allocate an extra buffer for de decrypted data
    csp_packet_t * buffer = NULL;

    buffer = csp_buffer_get(packet->length - crypto_box_BOXZEROBYTES + crypto_box_ZEROBYTES);
    if (buffer == NULL)
        goto out;

    // Extract NONCE from packet, then set padding to zero as required
    unsigned char nonce[crypto_box_NONCEBYTES];
    memset(nonce, 0, crypto_box_NONCEBYTES);
    memcpy(nonce, packet->data, crypto_box_BOXZEROBYTES);
    memset(packet->data, 0, crypto_box_BOXZEROBYTES);

    CRYPTO_TEST_PRINT_HEX(nonce);
    crypto_test_print_hex("packet->data", packet->data, packet->length);

    result = crypto_box_open_afternm(buffer->data, packet->data, packet->length, nonce, key_beforem);
    if(result != 0)
        goto out;

    // Message successfully decrypted, only then check if nonce was valid
    uint64_t nonce_counter = crypto_test_get_counter(nonce);
    printf("Counter: %lld - Nonce: %lld\n", *counter, nonce_counter);
    if(nonce_counter <= *counter) {
        printf("Nonce check failed\n");
        result = 1;
        goto out;
    }
    // Update counter with received value so that next sent value is higher
    *counter = nonce_counter;

    crypto_test_print_hex("buffer->data", buffer->data, packet->length);
    printf("    Decrypted packet: [%s]\n", buffer->data + crypto_secretbox_ZEROBYTES);

out:
    if (buffer != NULL)
        csp_buffer_free(buffer);

    return result;
}

void crypto_test_packet_handler(csp_packet_t * packet) {
    int result;
    csp_packet_t * reply_packet = NULL;

    uint8_t nonce_counter_parity = crypto_compare_pkey(_crypto_key_public, _crypto_key_test_public);
    printf("    --------\n    crypto_test_packet_handler %d bytes, parity=%d\n", packet->length, nonce_counter_parity);

    result = crypto_test_csp_decrypt_packet(packet, crypto_remote_beforem, &_crypto_remote_counter, nonce_counter_parity);
    csp_buffer_free(packet);
    if(result != 0)
        goto out;

    char reply[] = "SUCCESS";
    reply_packet = crypto_test_csp_encrypt_packet((uint8_t*)reply, sizeof(reply), _crypto_key_test_beforem, &_crypto_remote_counter, nonce_counter_parity);
    if(reply_packet == NULL)
        goto out;

    if (csp_sendto_reply(packet, reply_packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
        goto out;

out:
    if (reply_packet != NULL)
        csp_buffer_free(reply_packet);
}

int crypto_test_echo(uint8_t node, uint8_t * data, unsigned int size) {
    int result;
    uint32_t start, time, status = 0;

    uint8_t nonce_counter_parity = crypto_compare_pkey(_crypto_key_test_public, _crypto_key_public);
    printf("    crypto_test_echo %d bytes, parity=%d\n", size, nonce_counter_parity);

    // Counter
    start = csp_get_ms();

    // Open connection
    csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, node, CSP_DECRYPTOR_PORT, TEST_TIMEOUT, CSP_O_NONE);
    if (conn == NULL)
        return -1;

    csp_packet_t * packet = NULL;
    packet = crypto_test_csp_encrypt_packet(data, size, _crypto_key_test_beforem, &_crypto_test_counter, nonce_counter_parity);
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
    result = crypto_test_csp_decrypt_packet(packet, _crypto_key_test_beforem, &_crypto_test_counter, nonce_counter_parity);
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

void crypto_test_init(void) {
    // Debug
    crypto_test_generate_keys();

    // Pre-compute for for all public keys
    crypto_box_beforenm(crypto_remote_beforem, _crypto_remote_key, _crypto_key_secret);
    CRYPTO_TEST_PRINT_HEX(crypto_remote_beforem);

    /* Server */
    printf("Register Crypto Packet Handler\r\n");

    csp_socket_t *sock_crypto = csp_socket(CSP_SO_NONE);
    csp_socket_set_callback(sock_crypto, crypto_test_packet_handler);
    csp_bind(sock_crypto, CSP_DECRYPTOR_PORT);
}
