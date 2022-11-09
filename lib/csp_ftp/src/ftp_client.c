#include <stdio.h>
#include <csp/arch/csp_time.h>
#include <sys/types.h>

#include <string.h>

#include "csp_ftp/ftp_client.h"
#include "csp_ftp/ftp_server.h"

void ftp_download_file(int node, int timeout, const char * filename, int version, char * dataout, uint32_t dataout_size, uint32_t* recv_dataout_size)
{
	uint32_t time_begin = csp_get_ms();

	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, FTP_PORT_SERVER, timeout, CSP_O_RDP | CSP_O_CRC32);
	if (conn == NULL)
		return;

	csp_packet_t * packet = csp_buffer_get(sizeof(ftp_request_t));
	if (packet == NULL)
		return;

	ftp_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = FTP_SERVER_DOWNLOAD;
	packet->length = sizeof(ftp_request_t);

	size_t filename_len = strlen(filename);
	if( filename_len > MAX_PATH_LENGTH)
        return;

	strncpy(request->v1.address, filename, MAX_PATH_LENGTH);
	// This will only fail if MAX_PATH_LENGTH > UINT16_MAX
	request->v1.length = filename_len;

	/* Send request */
	csp_send(conn, packet);

	/* Go into download loop */
	int count = 0;
	while((packet = csp_read(conn, timeout)) != NULL) {
		printf("Client: client <- %d Bytes\n", packet->length);

		/* RX overflow check */
		if (count + packet->length > dataout_size) {
			printf("Client: Buffer overflow limit = %d, would end in %d\n", count + packet->length, dataout_size);
			csp_buffer_free(packet);
			break;
		}


		memcpy((void *) ((intptr_t) dataout + count), packet->data, packet->length);
		count += packet->length;
		csp_buffer_free(packet);
	}

	csp_close(conn);

	uint32_t time_total = csp_get_ms() - time_begin;

	printf("Client: Downloaded %u bytes in %.03f s at %u Bps\n", (unsigned int) count, time_total / 1000.0, (unsigned int) (count / ((float)time_total / 1000.0)) );
	*recv_dataout_size = count;

}

void ftp_upload_file(int node, int timeout, const char * filename, int version, char * datain, uint32_t length)
{
	uint32_t time_begin = csp_get_ms();

	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, FTP_PORT_SERVER, timeout, CSP_O_RDP | CSP_O_CRC32);
	if (conn == NULL)
		return;

	csp_packet_t * packet = csp_buffer_get(sizeof(ftp_request_t));
	if (packet == NULL)
		return;

	ftp_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = FTP_SERVER_UPLOAD;
	packet->length = sizeof(ftp_request_t);

	size_t filename_len = strlen(filename);
	if( filename_len > MAX_PATH_LENGTH)
        return;

	strncpy(request->v1.address, filename, MAX_PATH_LENGTH);
	// This will only fail if MAX_PATH_LENGTH > UINT16_MAX
	request->v1.length = filename_len;

	/* Send request */
	csp_send(conn, packet);

	unsigned int count = 0;
	while(count < length) {

		/* Prepare packet */
		csp_packet_t * packet = csp_buffer_get(FTP_SERVER_MTU);
		packet->length =  FTP_SERVER_MTU < length - count ? FTP_SERVER_MTU : length - count;

		memcpy(packet->data, (void *) ((intptr_t) datain + count), packet->length);
		count += packet->length;
		csp_send(conn, packet);

	}

	csp_close(conn);

	uint32_t time_total = csp_get_ms() - time_begin;

	printf("Client: Uploaded %u bytes in %.03f s at %u Bps\n", (unsigned int) count, time_total / 1000.0, (unsigned int) (count / ((float)time_total / 1000.0)) );

}