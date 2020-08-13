#include <param/param.h>
#include <param_config.h>


#include <stdio.h>//printf

extern uint8_t _crypto_key_public[16];
extern uint8_t _crypto_key_secret[16];
extern uint8_t _crypto_remote_key[16];
extern uint64_t _crypto_remote_counter;
extern uint64_t _crypto_fail_auth_count;
extern uint64_t _crypto_fail_nonce_count;

const PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_KEY_PUBLIC,        crypto_key_public,          PARAM_TYPE_DATA,   16, sizeof(uint8_t), PM_READONLY, NULL,                  "", &_crypto_key_public[0], NULL);
const PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_KEY_SECRET,        crypto_key_secret,          PARAM_TYPE_DATA,   16, sizeof(uint8_t), PM_READONLY, NULL,                  "", &_crypto_key_secret[0], NULL);
const PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_REMOTE_KEY,        crypto_remote_key,          PARAM_TYPE_DATA,   16, sizeof(uint8_t), PM_CONF,     NULL,                  "", &_crypto_remote_key[0], NULL);
const PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_REMOTE_COUNTER,    crypto_remote_counter,      PARAM_TYPE_UINT64, 1, sizeof(uint64_t), PM_READONLY, NULL,                  "", &_crypto_remote_counter, NULL);
const PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_FAUL_AUTH_COUNT,   crypto_fail_auth_count,     PARAM_TYPE_UINT64, 1, sizeof(uint64_t), PM_READONLY, NULL,                  "", &_crypto_fail_auth_count, NULL);
const PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_FAUL_NONCE_COUNT,  crypto_fail_nonce_count,    PARAM_TYPE_UINT64, 1, sizeof(uint64_t), PM_READONLY, NULL,                  "", &_crypto_fail_nonce_count, NULL);
