/*
 * vmem_client_slash.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */


#include <stdio.h>
#include <sys/stat.h>
#include <csp/csp.h>
#include <csp/csp_crc32.h>
#include <csp/arch/csp_time.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <vmem/vmem_client.h>

#include <slash/slash.h>
#include <slash/completer.h>
#include <slash/optparse.h>
#include <apm/csh_api.h>
#include "csh_internals.h"

static int vmem_client_slash_download(struct slash *slash)
{

	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int version = 1;
	// unsigned int offset = 0;
	unsigned int use_rdp = 1;

    optparse_t * parser = optparse_new("download", "<address> <length base10 or base16> <file>");
    optparse_add_help(parser);
    csh_add_node_option(parser, &node);
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 1)");
	// optparse_add_unsigned(parser, 'o', "offset", "NUM", 0, &offset, "byte offset in file (default = 0)");
	optparse_add_unsigned(parser, 'r', "use_rdp", "NUM", 0, &use_rdp, "rdp enable (default = 1)");

	rdp_opt_add(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	rdp_opt_set();

	/* Expect address */
	if (++argi >= slash->argc) {
		printf("missing address\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * endptr;
	uint64_t address = strtoul(slash->argv[argi], &endptr, 16);
	if (*endptr != '\0') {
		printf("Failed to parse address\n");
        optparse_del(parser);
		return SLASH_EUSAGE;
	}

	uint32_t length;
	int result = parse_length(slash, &argi, &length);
	if (result < 0) {
		optparse_del(parser);
		return result;
	}

	/* Expect filename */
	if (++argi >= slash->argc) {
		printf("missing filename\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * file;
	file = slash->argv[argi];


	char file_status[128] = {0};
	strncpy(file_status, file, 128-1);  // -1 to fit NULL byte
	strcat(file_status, "stat");
	FILE * fd_status = fopen(file_status, "r");

	/* Open file (truncate or create) or append to existing */
	FILE * fd = NULL;

	unsigned int offset = 0;
	/* Check if a previous download didn't finish */
	if (fd_status == NULL) {
		printf("No previous status file to resume from\n");
		fd = fopen(file, "w+");
	} else {
		if (fscanf(fd_status, "%u", &offset) != 1) {
			fprintf(stderr, "Failed to read offset from status file");
			optparse_del(parser);
			fclose(fd_status);
			return SLASH_EINVAL;
		}
		printf("Found status file\n");
		printf("Type 'yes' + enter to continue from offset: %u, else from beginning\n", offset);
		char * c = slash_readline(slash);

		if (strcmp(c, "yes") != 0) {
			fd = fopen(file, "w+");
			offset = 0;
		} else {
			fd = fopen(file, "a");
		}
		fclose(fd_status);
	}

	if (fd == NULL) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if(offset > length){
		printf("File status size offset %u is greater than length %u!\n", offset, length);
		optparse_del(parser);
		fclose(fd);
		return SLASH_EINVAL;
	}

	printf("Download %"PRIu32" bytes from %u addr 0x%"PRIX64" to %s with timeout %u version %u\n", length, node, address + offset, file, timeout, version);

	/* Allocate memory for reply */
	char * data = malloc(length - offset);

	int count = vmem_download(node, timeout, address + offset, length - offset, data, version, use_rdp);
	if(count < 0){
		printf("Connection failed\n");
		fclose(fd);
		free(data);
    	optparse_del(parser);
		return SLASH_EINVAL;
	}
	int res = SLASH_SUCCESS;
	printf(" - %.0f K\n", (count / 1024.0));

	/* Write data */
	int written = fwrite(data, 1, count, fd);
	fclose(fd);
	free(data);

	switch (count) {
	case CSP_ERR_TIMEDOUT:
		printf("Connection timeout\n");
		res = SLASH_EIO;
		break;
	case CSP_ERR_NOBUFS:
		printf("No more CSP buffers\n");
		res = SLASH_ENOMEM;
		break;
	default: 
		{
			/* If byte count is not equal to length - offset write to status file for resume later */
			if(count != (int)(length - offset)) {
				fd_status = fopen(file_status, "w+");
				if(fd_status) {
					rewind(fd_status);
					fprintf(fd_status, "%u", count + offset);
					fclose(fd_status);
					printf("Download didn't finish,  creating stat file %s. \nTo resume download rerun download cmd\n", file_status);
				} else {
					printf("Error creating stat file %s.\nTo resume download rerun download cmd\n", file_status);
				}
			} else {
				remove(file_status);
			}

			printf("wrote %d bytes to %s\n", written, file);
		}
		break;
	}

    optparse_del(parser);

	rdp_opt_reset();

	return res;
}
slash_command(download, vmem_client_slash_download, "<address> <length> <file>", "Download from VMEM to FILE");

static int vmem_client_slash_upload(struct slash *slash)
{
	int res = SLASH_SUCCESS;
	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int version = 1;
	unsigned int offset = 0;

    optparse_t * parser = optparse_new("upload", "<file> <address>");
    optparse_add_help(parser);
    csh_add_node_option(parser, &node);
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 1)");
	optparse_add_unsigned(parser, 'o', "offset", "NUM", 0, &offset, "byte offset in file (default = 0)");

	rdp_opt_add(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
		optparse_del(parser);
	    return SLASH_EINVAL;
    }

	rdp_opt_set();

	/* Expect filename */
	if (++argi >= slash->argc) {
		printf("missing filename\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * file;
	file = slash->argv[argi];

	/* Expect address */
	if (++argi >= slash->argc) {
		printf("missing address\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * endptr;
	uint64_t address = strtoull(slash->argv[argi], &endptr, 16);
	if (*endptr != '\0') {
		printf("Failed to parse address\n");
		optparse_del(parser);
		return SLASH_EUSAGE;
	}

	printf("Upload from %s to node %u addr 0x%"PRIX64" with timeout %u, version %u\n", file, node, address, timeout, version);

	/* Open file */
	FILE * fd = fopen(file, "r");
	if (fd == NULL){
		printf("File not found\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Read size */
	struct stat file_stat;
	stat(file, &file_stat);

	fseek(fd, offset, SEEK_SET);

	/* Copy to memory */
	char * data = malloc(file_stat.st_size);

	size_t size = fread(data, 1, file_stat.st_size - offset, fd);
	fclose(fd);

	address += offset;

	printf("File size %ld, offset %u, %zu bytes to upload to address 0x%"PRIX64"\n", file_stat.st_size, offset, size, address);

	csp_hex_dump("File head", data, 256);

	printf("Size %zu\n", size);

	uint32_t time_begin = csp_get_ms();
	int count = vmem_upload(node, timeout, address, data, size, version);
	uint32_t time_total = csp_get_ms() - time_begin;

	printf(" - %.0f K\n", (count / 1024.0));
	switch (count) {
	case CSP_ERR_TIMEDOUT:
		printf("Connection timeout\n");
		res = SLASH_EIO;
		break;
	case CSP_ERR_NOBUFS:
		printf("No more CSP buffers\n");
		res = SLASH_ENOMEM;
		break;
	default: 
		{
			if((size_t)count != size){
				unsigned int window_size = 0;
				csp_rdp_get_opt(&window_size, NULL, NULL, NULL, NULL, NULL);
				uint32_t suggested_offset = 0;
				if(((offset + count)) > ((window_size + 1) * VMEM_SERVER_MTU)){
					suggested_offset = (offset + count) - ((window_size + 1) * VMEM_SERVER_MTU);
				} 
				printf("Upload didn't complete, suggested offset to resume: %"PRIu32"\n", suggested_offset);
				res = SLASH_EIO;
			} else {
				printf("Uploaded %"PRIu32" bytes in %.03f s at %"PRIu32" Bps\n", count, time_total / 1000.0, (uint32_t)(count / ((float)time_total / 1000.0)) );
				res = SLASH_SUCCESS;
			}
		}
		break;
	}

	free(data);
	optparse_del(parser);

	rdp_opt_reset();

	return res;
}
slash_command_completer(upload, vmem_client_slash_upload, slash_path_completer, "<file> <address>", "Upload from FILE to VMEM");

static int vmem_client_slash_crc32(struct slash *slash) {

	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;
	unsigned int version = 1;
	char * file = NULL;

	optparse_t * parser = optparse_new("crc32", "<address> [length base10 or base16]");
	optparse_add_help(parser);
	csh_add_node_option(parser, &node);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
	optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 1)");
	optparse_add_string(parser, 'f', "file", "STR", &file, "file to compare crc32 against, length = file size");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Expect address */
	if (++argi >= slash->argc) {
		printf("missing address\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * endptr;
	uint64_t address = strtoul(slash->argv[argi], &endptr, 16);
	if (*endptr != '\0') {
		printf("Failed to parse address\n");
		optparse_del(parser);
		return SLASH_EUSAGE;
	}

	FILE * fd = NULL;
	uint32_t crc_file = 0;
	uint32_t length = 0;
	if(file == NULL){
		/* Expect length */
		if (++argi >= slash->argc) {
			printf("missing length\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		length = strtoul(slash->argv[argi], &endptr, 10);
		if (*endptr != '\0') {
			length = strtoul(slash->argv[argi], &endptr, 16);
			if (*endptr != '\0') {
				printf("Failed to parse length in base 10 or base 16\n");
				optparse_del(parser);
				return SLASH_EUSAGE;
			}
		}
	} else {
		fd = fopen(file, "r");
		if(fd == NULL){
			printf("file %s not found\n", file);
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		struct stat file_stat;
		stat(file, &file_stat);

		char * data = malloc(file_stat.st_size);
		if(data == NULL){
				printf("malloc failed\n");
				fclose(fd);
				return SLASH_ENOMEM;
		}

		unsigned int size = fread(data, 1, file_stat.st_size, fd);
		if(size != file_stat.st_size){
			printf("Read file failed\n");
			fclose(fd);
			return SLASH_EINVAL;
		}
		fclose(fd);
		length = size;
		crc_file = csp_crc32_memory((uint8_t*)data, length);
		printf("  File CRC32: 0x%08"PRIX32"\n", crc_file);
	}


	printf("  Calculate CRC32 from %u addr 0x%"PRIX64" with timeout %u, version %u\n", node, address, timeout, version);

	uint32_t crc = 0;
	int res = vmem_client_calc_crc32(node, timeout, address, length, &crc, version);

	if(file){
		if (crc_file == crc) {
			printf("\033[32m\n");
			printf("  Success\n");
			printf("\033[0m\n");
		} else {
			printf("\033[31m\n");
			printf("  Failure: %"PRIX32" != %"PRIX32"\n", crc, crc_file);
			printf("\033[0m\n");
		}
	}

	optparse_del(parser);
	if (res) {
		if (res == -1) {
			return SLASH_ENOMEM;
		} else if (res == -2) {
			printf("\033[31m\n");
			printf("Timed out on crc32 response\n");
			printf("\033[0m\n");
			return SLASH_EIO;
		}
	}

	return SLASH_SUCCESS;
}
slash_command(crc32, vmem_client_slash_crc32, "<address> <length>", "Calculate CRC32 on a VMEM area");

static int vmem_client_rdp_options(struct slash *slash) {

    optparse_t * parser = optparse_new("rdp opt", "");
    optparse_add_help(parser);

    csp_rdp_get_opt(&rdp_dfl_window, &rdp_dfl_conn_timeout, &rdp_dfl_packet_timeout, &rdp_dfl_delayed_acks, &rdp_dfl_ack_timeout, &rdp_dfl_ack_count);

	printf("Current RDP options window: %u, conn_timeout: %u, packet_timeout: %u, ack_timeout: %u, ack_count: %u\n", 
        rdp_dfl_window, rdp_dfl_conn_timeout, rdp_dfl_packet_timeout, rdp_dfl_ack_timeout, rdp_dfl_ack_count);

	optparse_add_unsigned(parser, 'w', "window", "NUM", 0, &rdp_dfl_window, "rdp window (default = 3 packets)");
	optparse_add_unsigned(parser, 'c', "conn_timeout", "NUM", 0, &rdp_dfl_conn_timeout, "rdp connection timeout in ms (default = 10 seconds)");
	optparse_add_unsigned(parser, 'p', "packet_timeout", "NUM", 0, &rdp_dfl_packet_timeout, "rdp packet timeout in ms (default = 5 seconds)");
	optparse_add_unsigned(parser, 'k', "ack_timeout", "NUM", 0, &rdp_dfl_ack_timeout, "rdp max acknowledgement interval in ms (default = 2 seconds)");
	optparse_add_unsigned(parser, 'a', "ack_count", "NUM", 0, &rdp_dfl_ack_count, "rdp ack for each (default = 2 packets)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    printf("Setting RDP options window: %u, conn_timeout: %u, packet_timeout: %u, ack_timeout: %u, ack_count: %u\n",
        rdp_dfl_window, rdp_dfl_conn_timeout, rdp_dfl_packet_timeout, rdp_dfl_ack_timeout, rdp_dfl_ack_count);

	csp_rdp_set_opt(rdp_dfl_window, rdp_dfl_conn_timeout, rdp_dfl_packet_timeout, 1, rdp_dfl_ack_timeout, rdp_dfl_ack_count);

	return SLASH_SUCCESS;
}
slash_command_sub(rdp, opt, vmem_client_rdp_options, NULL, "Set RDP options to use in stream and file transfers");

extern unsigned int rdp_tmp_window;
extern unsigned int rdp_tmp_conn_timeout;
extern unsigned int rdp_tmp_packet_timeout;
extern unsigned int rdp_tmp_delayed_acks;
extern unsigned int rdp_tmp_ack_timeout;
extern unsigned int rdp_tmp_ack_count;

void rdp_opt_add(optparse_t * parser) {

	rdp_tmp_window = rdp_dfl_window;
	rdp_tmp_conn_timeout = rdp_dfl_conn_timeout;
	rdp_tmp_packet_timeout = rdp_dfl_packet_timeout;
	rdp_tmp_delayed_acks = rdp_dfl_delayed_acks;
	rdp_tmp_ack_timeout = rdp_dfl_ack_timeout;
	rdp_tmp_ack_count = rdp_dfl_ack_count;

	optparse_add_unsigned(parser, 'w', "window", "NUM", 0, &rdp_tmp_window, "rdp window (default = 3 packets)");
	optparse_add_unsigned(parser, 'c', "conn_timeout", "NUM", 0, &rdp_tmp_conn_timeout, "rdp connection timeout (default = 10000)");
	optparse_add_unsigned(parser, 'p', "packet_timeout", "NUM", 0, &rdp_tmp_packet_timeout, "rdp packet timeout (default = 5000)");
	optparse_add_unsigned(parser, 'k', "ack_timeout", "NUM", 0, &rdp_tmp_ack_timeout, "rdp max acknowledgement interval (default = 2000)");
	optparse_add_unsigned(parser, 'a', "ack_count", "NUM", 0, &rdp_tmp_ack_count, "rdp ack for each (default = 2 packets)");
}

