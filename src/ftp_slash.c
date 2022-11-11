#include <stdio.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <csp_autoconfig.h>
#include <csp/csp_hooks.h>
#include <sys/types.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>

#include "csp_ftp/ftp_client.h"
#include "csp_ftp/ftp_server.h"

static int slash_csp_upload_file(struct slash *slash)
{
    char* local_filename = "";
    char* remote_filename = "";
    unsigned int timeout = FTP_CLIENT_TIMEOUT;
    unsigned int node = slash_dfl_node;

    optparse_t * parser = optparse_new("upload_file", "");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timout", "NUM", 0, &timeout, "timout for connection (default = FTP_CLIENT_TIMEOUT)");
    optparse_add_string(parser, 'l', "file", "FILE", &local_filename, "locale path to file");
    optparse_add_string(parser, 'r', "file", "FILE", &remote_filename, "remote path to file");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (strlen(local_filename) == 0) {
        printf("Local file path cannot be empty\n");
        return SLASH_EINVAL;
    }

    if (strlen(remote_filename) == 0) {
        printf("Remote file path cannot be empty\n");
        return SLASH_EINVAL;
    }

    printf("%s\n", local_filename);

    FILE *f = fopen(local_filename, "rb");
    
    if(f == NULL)
        return 1;
    
    fseek(f, 0L, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0L, SEEK_SET);	
    
    char *file_content = (char*)calloc(file_size, sizeof(char));
    
    if(file_content == NULL)
        return 1;
    
    fread(file_content, sizeof(char), file_size, f);
    fclose(f);
    
    printf("Client: Uploading file of %ld bytes\n", file_size);
    //void ftp_upload_file(int node, int timeout, const char * filename, int version, char * datain, uint32_t length);
    ftp_upload_file(node, timeout, remote_filename, 1, file_content, file_size);
    printf("Client: Transfer complete\n");

    /* free the memory we used for the buffer */
    free(file_content);

	return SLASH_SUCCESS;
}

slash_command(upload_file, slash_csp_upload_file, NULL, "Upload a file to the target node");

static int slash_csp_download_file(struct slash *slash)
{
	char* local_filename = "";
    char* remote_filename = "";
    unsigned int timeout = FTP_CLIENT_TIMEOUT;
    unsigned int node = slash_dfl_node;

    optparse_t * parser = optparse_new("upload_file", "");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timout", "NUM", 0, &timeout, "timout for connection (default = FTP_CLIENT_TIMEOUT)");
    optparse_add_string(parser, 'r', "file", "FILE", &remote_filename, "remote path to file");
    optparse_add_string(parser, 'l', "file", "FILE", &local_filename, "locale path to file");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    /**/

    if (strlen(local_filename) == 0) {
        printf("Local file path cannot be empty\n");
        return SLASH_EINVAL;
    }

    if (strlen(remote_filename) == 0) {
        printf("Remote file path cannot be empty\n");
        return SLASH_EINVAL;
    }

    printf("Client: Downloading file\n");
    char *file_content = NULL;
    int file_content_size = 0;

    ftp_download_file(node, timeout, remote_filename, 1, &file_content, &file_content_size);
    printf("Client: Transfer complete\n");

    if (file_content_size == 0) {
        printf("Client Recieved empty file\n");
        return SLASH_SUCCESS;
    }

    FILE* f = fopen(local_filename, "wb");
    fwrite(file_content, file_content_size, 1, f);
    fclose(f);

    printf("Client Saved file to %s\n", local_filename);

    free(file_content);

	return SLASH_SUCCESS;
}

slash_command(download_file, slash_csp_download_file, NULL, "Download a file from the target node");


static int slash_csp_list_file(struct slash *slash)
{
    char* remote_directory = "";
    unsigned int timeout = FTP_CLIENT_TIMEOUT;
    unsigned int node = slash_dfl_node;

    optparse_t * parser = optparse_new("upload_file", "");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timout", "NUM", 0, &timeout, "timout for connection (default = FTP_CLIENT_TIMEOUT)");
    optparse_add_string(parser, 'd', "dir", "FILE", &remote_directory, "remote path to directory");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (strlen(remote_directory) == 0) {
        printf("Remote file path cannot be empty\n");
        return SLASH_EINVAL;
    }
    
    printf("Client: List file\n");

    char* filenames;
    int file_count;
    ftp_list_files(node, timeout, remote_directory, 1, &filenames, &file_count);
    printf("Client: Transfer complete\n");

	printf("Client: Recieved %d files of size %d\n", file_count, file_count * MAX_PATH_LENGTH);
	for (int i = 0; i < file_count; i++) {
		printf("Client: - %s\n", filenames + MAX_PATH_LENGTH * i);
	}

    free(filenames);
	return SLASH_SUCCESS;
}

slash_command(list_file, slash_csp_list_file, NULL, "List all files in directory");