/*
 * param_sniffer.h
 *
 *  Created on: Aug 12, 2020
 *      Author: joris
 */

#ifndef SRC_CRYPTO_TEST_H_
#define SRC_CRYPTO_TEST_H_

void crypto_test_init(void);
int crypto_test_echo(uint8_t node, uint8_t * data, unsigned int size);
void crypto_test_generate_local_key(void);
void crypto_test_key_refresh(void);

#endif /* SRC_CRYPTO_TEST_H_ */
