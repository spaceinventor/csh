/*
 * param_sniffer.c
 *
 *  Created on: Aug 12, 2020
 *      Author: joris
 */

#include "crypto.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param_config.h>

#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <csp/arch/csp_thread.h>

#include "tweetnacl.h"
#include "crypto_param.h"

// Future Params for server
static uint8_t _crypto_key_public[crypto_box_PUBLICKEYBYTES];
static uint8_t _crypto_key_secret[crypto_box_SECRETKEYBYTES];
static uint8_t _crypto_key_remote[crypto_box_PUBLICKEYBYTES];
static uint8_t _crypto_beforenm[crypto_box_BEFORENMBYTES];

uint64_t _crypto_nonce;
uint16_t _crypto_fail_auth_count;
uint16_t _crypto_fail_nonce_count;

PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_REMOTE_COUNTER,    crypto_nonce,      PARAM_TYPE_UINT64,  1, sizeof(uint64_t), PM_READONLY, NULL,                  "", &_crypto_nonce, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_FAUL_AUTH_COUNT,   crypto_fail_auth_count,     PARAM_TYPE_UINT16,  1, sizeof(uint16_t), PM_READONLY, NULL,                  "", &_crypto_fail_auth_count, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_FAUL_NONCE_COUNT,  crypto_fail_nonce_count,    PARAM_TYPE_UINT16,  1, sizeof(uint16_t), PM_READONLY, NULL,                  "", &_crypto_fail_nonce_count, NULL);

static uint8_t crypto_compare_pkey(uint8_t * own_key, uint8_t * other_key) {
    int result = memcmp(own_key, other_key, crypto_box_PUBLICKEYBYTES);
    if(result > 0) {
        return 1;
    }
	return 0;
}

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
int crypto_encrypt_with_zeromargin(uint8_t * msg_begin, uint8_t msg_len, uint8_t * ciphertext_out) {

	/* Increment nonce */
	uint8_t nonce_counter_parity = crypto_compare_pkey(_crypto_key_remote, _crypto_key_public);
	_crypto_nonce = (_crypto_nonce & (~1UL)) + 1 + nonce_counter_parity;
	//printf("nonce tx: %"PRIu64"\n", _crypto_nonce);

	/* Pack nonce into 24-bytes format, expected by NaCl */
	unsigned char nonce[crypto_box_NONCEBYTES] = {};
	memcpy(nonce, &_crypto_nonce, sizeof(uint64_t));
	//csp_hex_dump("nonce", nonce, crypto_box_NONCEBYTES);

	/* Make room for zerofill at the beginning of message */
	uint8_t * zerofill_in = msg_begin - crypto_secretbox_ZEROBYTES;
	memset(zerofill_in, 0, crypto_secretbox_ZEROBYTES);

	/* Make room for zerofill at the beginning of message */
	uint8_t * zerofill_out = ciphertext_out - crypto_secretbox_BOXZEROBYTES;
	memset(zerofill_out, 0, crypto_secretbox_BOXZEROBYTES);

	if (crypto_box_afternm(zerofill_out, zerofill_in, crypto_secretbox_ZEROBYTES + msg_len, nonce, _crypto_beforenm) != 0) {
		return -1;
	}

	/* Add nonce at the end of the packet */
	memcpy(zerofill_out + msg_len + crypto_secretbox_ZEROBYTES, nonce, sizeof(uint64_t));

	return msg_len + crypto_secretbox_BOXZEROBYTES + sizeof(uint64_t);

}

int crypto_decrypt_with_zeromargin(uint8_t * ciphertext_in, uint8_t ciphertext_len, uint8_t * msg_out) {

	/* Receive nonce */
	unsigned char nonce[crypto_box_NONCEBYTES] = {};
	memcpy(&nonce, ciphertext_in + ciphertext_len - sizeof(uint64_t), sizeof(uint64_t));
	ciphertext_len = ciphertext_len - sizeof(uint64_t);

	//csp_hex_dump("nonce", nonce, crypto_box_NONCEBYTES);
	//csp_hex_dump("cihper in", ciphertext_in, ciphertext_len	);

	/* Make room for zerofill at the beginning of message */
	uint8_t * zerofill_in = ciphertext_in - crypto_secretbox_BOXZEROBYTES;
	memset(zerofill_in, 0, crypto_secretbox_BOXZEROBYTES);

	/* Make room for zerofill at the beginning of message */
	uint8_t * zerofill_out = msg_out - crypto_secretbox_ZEROBYTES;
	memset(zerofill_out, 0, crypto_secretbox_ZEROBYTES);

	//csp_hex_dump("zerofill_in", zerofill_in, crypto_secretbox_BOXZEROBYTES + ciphertext_len);
	//csp_hex_dump("zerofill_out", zerofill_out, crypto_secretbox_ZEROBYTES);
	//csp_hex_dump("beforenm", _crypto_beforenm, sizeof(_crypto_beforenm));

	/* Decryption */
	if(crypto_box_open_afternm(zerofill_out, zerofill_in, crypto_secretbox_BOXZEROBYTES + ciphertext_len, nonce, _crypto_beforenm) != 0) {
		_crypto_fail_auth_count++;
		return -1;
	}

    /* Message successfully decrypted, check for valid nonce */
    uint64_t nonce_counter;
    memcpy(&nonce_counter, nonce, sizeof(uint64_t));
    if(nonce_counter <= _crypto_nonce) {
    	_crypto_fail_nonce_count++;
        return -1;
    }

    /* Update counter with received value so that next sent value is higher */
    _crypto_nonce = nonce_counter;

    /* Return useable length */
	return ciphertext_len - crypto_secretbox_BOXZEROBYTES;

}

void crypto_key_refresh(void) {

	/* Read keys from vmem/config file */
	param_get_data(&crypto_key_public, _crypto_key_public, crypto_box_PUBLICKEYBYTES);
	param_get_data(&crypto_key_secret, _crypto_key_secret, crypto_box_SECRETKEYBYTES);
	param_get_data(&crypto_key_remote, _crypto_key_remote, crypto_box_PUBLICKEYBYTES);

	//csp_hex_dump("public", _crypto_key_public, sizeof(_crypto_key_public));
	//csp_hex_dump("secret", _crypto_key_secret, sizeof(_crypto_key_secret));
	//csp_hex_dump("remote", _crypto_key_remote, sizeof(_crypto_key_remote));

	/* Pre compute stuff */
    crypto_box_beforenm(_crypto_beforenm, _crypto_key_remote, _crypto_key_secret);
    //csp_hex_dump("beforenm", _crypto_beforenm, sizeof(_crypto_beforenm));

}

void crypto_generate_local_key(void) {
    int result;

    static uint8_t new_public[crypto_box_PUBLICKEYBYTES];
    static uint8_t new_secret[crypto_box_SECRETKEYBYTES];

	result = crypto_box_keypair(new_public, new_secret);
    if(result != 0) {
    	printf("FAIL: crypto box keypair\n");
    	return;
	}

    param_set_data(&crypto_key_public, new_public, sizeof(new_public));
    param_set_data(&crypto_key_secret, new_secret, sizeof(new_secret));

    param_print(&crypto_key_public, 0, 0, 0, 2);
    param_print(&crypto_key_secret, 0, 0, 0, 2);

}

