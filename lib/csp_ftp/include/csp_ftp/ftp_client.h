/*
 * vmem_client.h
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#ifndef CSP_FTP_INCLUDE_FTP_CLIENT_H_
#define CSP_FTP_INCLUDE_FTP_CLIENT_H_

#include <stdint.h>

void ftp_download_file(int node, int timeout, const char * filename, int version, char * dataout, uint32_t dataout_size, uint32_t* recv_dataout_size);
void ftp_upload_file(int node, int timeout, const char * filename, int version, char * datain, uint32_t length);
// void ftp_client_list(int node, int timeout, int version);

#endif /* CSP_FTP_INCLUDE_FTP_CLIENT_H_ */
