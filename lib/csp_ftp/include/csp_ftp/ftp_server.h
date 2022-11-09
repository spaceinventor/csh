/*
 * vmem_server.h
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#ifndef LIB_CSP_FTP_INCLUDE_FTP_SERVER_H_
#define LIB_CSP_FTP_INCLUDE_FTP_SERVER_H_

#include <csp/csp.h>

#define FTP_SERVER_TIMEOUT 30000
#define FTP_SERVER_MTU 192
#define FTP_PORT_SERVER 12
#define FTP_VERSION 1

#define MAX_PATH_LENGTH 100

typedef enum {
	FTP_SERVER_UPLOAD,
	FTP_SERVER_DOWNLOAD
} ftp_request_type;

// Header for any request to the server
typedef struct {
	uint8_t version;
	uint8_t type;
	union {
		struct {
            // Path to the file
			char address[MAX_PATH_LENGTH];
            // Length of the path
            uint16_t length;
		} v1;
	};
} __attribute__((packed)) ftp_request_t;

typedef struct {
	uint32_t vaddr;
	uint32_t size;
	uint8_t vmem_id;
	uint8_t type;
	char name[5];
} __attribute__((packed)) ftp_list_t;

void ftp_server_handler(csp_conn_t * conn);
void handle_server_download(csp_conn_t * conn, ftp_request_t * request);
void handle_server_upload(csp_conn_t * conn, ftp_request_t * request);
void ftp_server_loop(void * param);

#endif /* LIB_CSP_FTP_FTP_INCLUDE_FTP_SERVER_H_ */
