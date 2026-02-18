#pragma once
#include <stdint.h>

#define VMEM_SERVER_MOVE 0x90
#define VMEM_SERVER_DECOMPRESS 0x91
#define VMEM_SERVER_COMPRESS 0x92

typedef enum {
	VMEM_CODEC_SUCCESS,
	VMEM_CODEC_FAIL,
} vmem_response;

typedef struct vmem_codec_response_s {
	int32_t return_code;
	uint32_t length;
} __attribute__((packed)) vmem_codec_response_t;

typedef struct {
	uint64_t src_address;
	uint64_t dst_address;
	uint32_t length;
} __attribute__((packed)) vmem_request_codec_t;

/**
 * @brief User-definable decompression function.
 * @param[in] dst_addr Destination address.
 * @param[out] dst_len Pointer to store the final decompressed size.
 * @param[in] src_addr Source address of the compressed data.
 * @param[in] src_len Length of the compressed data.
 * @return 0 on success, negative on error.
 */
typedef int (*vmem_decompress_fnc_t)(uint64_t dst_addr, uint32_t *dst_len, uint64_t src_addr, uint32_t src_len);

/**
 * @brief User-definable compression function.
 */
typedef int (*vmem_compress_fnc_t)(uint64_t dst_addr, uint32_t *dst_len, uint64_t src_addr, uint32_t src_len);

/**
 * @brief Register a decompression function for the VMEM server.
 * @param[in] fnc The function to handle decompression requests.
 */
void vmem_server_set_decompress_fnc(vmem_decompress_fnc_t fnc);

/**
 * @brief Register a compression function for the VMEM server.
 * @param[in] fnc The function to handle compression requests.
 */
void vmem_server_set_compress_fnc(vmem_compress_fnc_t fnc);


vmem_decompress_fnc_t vmem_server_get_decompress_fnc(void);

vmem_compress_fnc_t vmem_server_get_compress_fnc(void);

int vmem_client_compress(int node, int timeout, uint64_t src_address, uint64_t dst_address, uint32_t length, int version);
int vmem_client_decompress(int node, int timeout, uint64_t src_address, uint64_t dst_address, uint32_t length, int version);

int vmem_codec_server_init(void);
