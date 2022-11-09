/*
 * vmem_server.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <sys/types.h>
#include <csp/arch/csp_time.h>

#include <csp_ftp/ftp_server.h>


void ftp_server_handler(csp_conn_t * conn)
{
	// Read request
	csp_packet_t * packet = csp_read(conn, FTP_SERVER_TIMEOUT);
	if (packet == NULL)
		return;

	// Copy data from request
	ftp_request_t * request = (void *) packet->data;

    // The request is not of an accepted type
    if (request->version > FTP_VERSION) {
        printf("Server: Unknown version\n");
        return;
    }

	int type = request->type;

	if (type == FTP_SERVER_DOWNLOAD) {
        printf("Server: Handling download request\n");
        handle_server_download(conn, request);
	} else if (request->type == FTP_SERVER_UPLOAD) {
        printf("Server: Handling upload request\n");
		handle_server_upload(conn, request);
    } else {
        printf("Server: Unknown request\n");
	}

    csp_buffer_free(packet);
    
}

void handle_server_download(csp_conn_t * conn, ftp_request_t * request) {
    char * address = request->v1.address;

    printf("Server: Starts sending back %s\n", address);
    FILE* f = fopen(address, "rb");

    fseek(f, 0L, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0L, SEEK_SET);	

    printf("Server: Sending %ld Bytes\n", file_size);

    while (file_size > 0) {

        /* Prepare packet */
        csp_packet_t * packet = csp_buffer_get(FTP_SERVER_MTU);
        if (packet == NULL) {
            break;
        }
        //packet->length = file_size < FTP_SERVER_MTU ? file_size : FTP_SERVER_MTU;
        packet->length = file_size < FTP_SERVER_MTU ? file_size : FTP_SERVER_MTU;
        
        /* Get data */
        fread(packet->data, packet->length, 1, f);

        file_size = file_size - packet->length;

        printf("Server: server -> %d Bytes\n", packet->length);
        csp_send(conn, packet);

        csp_buffer_free(packet);
    }

    fclose(f);
}

void handle_server_upload(csp_conn_t * conn, ftp_request_t * request) {
    char * address = request->v1.address;

    printf("Server: Saving result to %s\n", address);
    FILE* f = fopen(address, "wb");

    long file_size = 0;
    csp_packet_t * packet;
    while((packet = csp_read(conn, FTP_SERVER_TIMEOUT)) != NULL) {
        printf("Server: server <- %d Bytes\n", packet->length);
        
        fwrite(packet->data, packet->length, 1, f);
        csp_buffer_free(packet);
        
        file_size = file_size + packet->length;
    }

    printf("Server: Received %ld Bytes\n", file_size);

    fclose(f);
}

void ftp_server_loop(void * param) {

	/* Statically allocate a listener socket */
	static csp_socket_t ftp_server_socket = {0};

	/* Bind all ports to socket */
	csp_bind(&ftp_server_socket, FTP_PORT_SERVER);

	/* Create 10 connections backlog queue */
	csp_listen(&ftp_server_socket, 10);

	/* Pointer to current connection and packet */
	csp_conn_t *conn;

	/* Process incoming connections */
	while (1) {

		/* Wait for connection, 10000 ms timeout */
		if ((conn = csp_accept(&ftp_server_socket, CSP_MAX_DELAY)) == NULL) {
			continue;
        }

        printf("Server: Recieved connection\n");

		/* Handle RDP service differently */
		if (csp_conn_dport(conn) == FTP_PORT_SERVER) {
			ftp_server_handler(conn);
			csp_close(conn);
			continue;
		} else {
            printf("Server: Unrecognized connection type\n");
        }

		/* Close current connection, and handle next */
		csp_close(conn);

	}

}

