#include <param/param.h>
#include <param_config.h>

#include "crypto_test.h"

void key_refresh_callback(param_t * param, int idx) {
	crypto_test_key_refresh();
}

extern uint32_t _crypto_remote_counter;
extern uint32_t _crypto_fail_auth_count;
extern uint32_t _crypto_fail_nonce_count;

PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_REMOTE_COUNTER,    crypto_remote_counter,      PARAM_TYPE_UINT32,  1, sizeof(uint32_t), PM_READONLY, NULL,                  "", &_crypto_remote_counter, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_FAUL_AUTH_COUNT,   crypto_fail_auth_count,     PARAM_TYPE_UINT32,  1, sizeof(uint32_t), PM_READONLY, NULL,                  "", &_crypto_fail_auth_count, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CRYPTO_FAUL_NONCE_COUNT,  crypto_fail_nonce_count,    PARAM_TYPE_UINT32,  1, sizeof(uint32_t), PM_READONLY, NULL,                  "", &_crypto_fail_nonce_count, NULL);

extern vmem_t vmem_crypto;

PARAM_DEFINE_STATIC_VMEM(PARAMID_CRYPTO_KEY_PUBLIC,       crypto_key_public,          PARAM_TYPE_DATA,   32, sizeof(uint8_t),  PM_READONLY, key_refresh_callback,                  "", crypto, 100, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_CRYPTO_KEY_SECRET,       crypto_key_secret,          PARAM_TYPE_DATA,   32, sizeof(uint8_t),  PM_READONLY, key_refresh_callback,                  "", crypto, 200, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_CRYPTO_KEY_REMOTE,       crypto_key_remote,          PARAM_TYPE_DATA,   32, sizeof(uint8_t),  PM_CONF,     key_refresh_callback,                  "", crypto, 300, NULL);
