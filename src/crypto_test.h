/*
 * param_sniffer.h
 *
 *  Created on: Aug 12, 2020
 *      Author: joris
 */

#ifndef SRC_CRYPTO_TEST_H_
#define SRC_CRYPTO_TEST_H_

#include <csp/csp.h>

void crypto_test_init(void);
int crypto_test_echo(uint8_t node, uint8_t * data, unsigned int size);
void crypto_test_generate_local_key(void);
void crypto_test_key_refresh(void);

int crypto_encrypt_with_zeromargin(uint8_t * msg_begin, uint8_t msg_len, uint8_t * ciphertext_out);
int crypto_decrypt_with_zeromargin(uint8_t * ciphertext_in, uint8_t ciphertext_len, uint8_t * msg_out);

#endif /* SRC_CRYPTO_TEST_H_ */
