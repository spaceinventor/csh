#include "vmem_codec.h"

#include <stdio.h>
#include <endian.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>

#include <vmem/vmem_client.h>

vmem_decompress_fnc_t decompress_handler = (vmem_decompress_fnc_t)NULL;
vmem_compress_fnc_t compress_handler = (vmem_decompress_fnc_t)NULL;

void vmem_server_set_decompress_fnc(vmem_decompress_fnc_t fnc) {
    decompress_handler = fnc;
}

void vmem_server_set_compress_fnc(vmem_compress_fnc_t fnc) {
    compress_handler = fnc;
}

vmem_decompress_fnc_t vmem_server_get_decompress_fnc(void) {
    return decompress_handler;
}

vmem_compress_fnc_t vmem_server_get_compress_fnc(void) {
    return compress_handler;
}

static int vmem_codec_request_handler(csp_conn_t *conn, csp_packet_t *packet, void *context) {
	(void)context;

	vmem_request_t * request = (void *) packet->data;

	if (request->type == VMEM_SERVER_DECOMPRESS) {

		vmem_decompress_fnc_t decompress_fnc = vmem_server_get_decompress_fnc();

		if (decompress_fnc == NULL) {
			printf("ERROR: No VMEM codec function defined\n");
			csp_buffer_free(packet);
			return -1;
		}

	    vmem_request_codec_t * codec = (vmem_request_codec_t *) &request->data;

		uint64_t src_addr = be64toh(codec->src_address);
		uint32_t src_len = be32toh(codec->length);
		uint64_t dst_addr = be64toh(codec->dst_address);

		uint32_t final_len;
		packet->data32[0] = decompress_fnc(dst_addr, &final_len, src_addr, src_len);
		packet->data32[1] = htobe32(final_len);
		packet->length = 8;
		csp_send(conn, packet);
	}

	return 0;
}

static int vmem_client_codec(int node, int timeout, uint64_t src_address, uint64_t dst_address, uint32_t length, int type, int version) {

	int res = -1;

	/* Establish connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_CRC32);
	if (conn == NULL)
		return res;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL)
		return res;

	vmem_request_t * request = (void *) packet->data;
    vmem_request_codec_t * codec = (vmem_request_codec_t *) &request->data;
	request->version = version;
	request->type = type;
    codec->src_address = htobe64(src_address);
    codec->dst_address = htobe64(dst_address);
    codec->length = htobe32(length);
	packet->length = sizeof(vmem_request_hdr_t) + sizeof(vmem_request_codec_t);


	/* Send request */
	csp_send(conn, packet);

	/* Wait for the reponse from the server */
	/* Blocking read */
	/* TODO request with unpacked length */
	packet = csp_read(conn, timeout);
	if (packet) {
		if (be32toh(packet->data32[0]) == VMEM_CODEC_SUCCESS) {
			printf("Codec success from 0x%08"PRIX64" to 0x%08"PRIX64" %u bytes\n", 
				src_address, dst_address, (unsigned int) length);
			res = 0;
		} else {
			res = -3;
			printf("VMEM Codec failing with error %"PRIu32"\n", be32toh(packet->data32[0]));
		}
		csp_buffer_free(packet);
	} else {
		/* Timeout */
		res = -2;
	}

	csp_close(conn);

	return res;
}

int vmem_client_compress(int node, int timeout, uint64_t src_address, uint64_t dst_address, uint32_t length, int version) {
	return vmem_client_codec(node, timeout, src_address, dst_address, length, VMEM_SERVER_COMPRESS, version);
}

int vmem_client_decompress(int node, int timeout, uint64_t src_address, uint64_t dst_address, uint32_t length, int version) {
	return vmem_client_codec(node, timeout, src_address, dst_address, length, VMEM_SERVER_DECOMPRESS, version);
}

static vmem_handler_obj_t vmem_codec_context;

int vmem_codec_server_init(void) {

    vmem_server_bind_type(VMEM_SERVER_DECOMPRESS, vmem_codec_request_handler, &vmem_codec_context, NULL);

	return 0;
}
