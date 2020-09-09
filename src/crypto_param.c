#include <param/param.h>
#include <param_config.h>

#include "crypto.h"

void key_refresh_callback(param_t * param, int idx) {
	crypto_test_key_refresh();
}

extern vmem_t vmem_crypto;

PARAM_DEFINE_STATIC_VMEM(PARAMID_CRYPTO_KEY_PUBLIC,       crypto_key_public,          PARAM_TYPE_DATA,   32, sizeof(uint8_t),  PM_READONLY, key_refresh_callback,                  "", crypto, 100, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_CRYPTO_KEY_SECRET,       crypto_key_secret,          PARAM_TYPE_DATA,   32, sizeof(uint8_t),  PM_READONLY, key_refresh_callback,                  "", crypto, 200, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_CRYPTO_KEY_REMOTE,       crypto_key_remote,          PARAM_TYPE_DATA,   32, sizeof(uint8_t),  PM_CONF,     key_refresh_callback,                  "", crypto, 300, NULL);
