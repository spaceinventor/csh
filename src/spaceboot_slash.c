

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

#include <slash/slash.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>

#include <vmem/vmem.h>
#include <vmem/vmem_client.h>
#include <vmem/vmem_server.h>
#include <vmem/vmem_ram.h>

#include <csp/csp.h>
#include <csp/csp_cmp.h>

static int ping(int node) {

	struct csp_cmp_message message = {};
	if (csp_cmp_ident(node, 1000, &message) != CSP_ERR_NONE) {
		printf("Cannot ping system\n");
		return SLASH_EINVAL;
	}
	printf("  | %s\n  | %s\n  | %s\n  | %s %s\n", message.ident.hostname, message.ident.model, message.ident.revision, message.ident.date, message.ident.time);
	return SLASH_SUCCESS;
}

static int reset_to_flash(int node, int flash, int times, int type) {

	param_t * boot_img[4];
	/* Setup remote parameters */
	boot_img[0] = param_list_create_remote(21, node, PARAM_TYPE_UINT8, PM_CONF, 0, "boot_img0", 10);
	boot_img[1] = param_list_create_remote(20, node, PARAM_TYPE_UINT8, PM_CONF, 0, "boot_img1", 10);
	boot_img[2] = param_list_create_remote(22, node, PARAM_TYPE_UINT8, PM_CONF, 0, "boot_img2", 10);
	boot_img[3] = param_list_create_remote(23, node, PARAM_TYPE_UINT8, PM_CONF, 0, "boot_img3", 10);

	printf("  Switching to flash %d\n", flash);
	printf("  Will run this image %d times\n", times);

	char queue_buf[50];
	param_queue_t queue;
	param_queue_init(&queue, queue_buf, 50, 0, PARAM_QUEUE_TYPE_SET, 2);

	uint8_t zero = 0;
	param_queue_add(&queue, boot_img[0], 0, &zero);
	param_queue_add(&queue, boot_img[1], 0, &zero);
	if (type == 1) {
		param_queue_add(&queue, boot_img[2], 0, &zero);
		param_queue_add(&queue, boot_img[3], 0, &zero);
	}
	param_queue_add(&queue, boot_img[flash], 0, &times);
	param_push_queue(&queue, 1, node, 1000);

	printf("  Rebooting");
	csp_reboot(node);
	int step = 25;
	int ms = 1000;
	while (ms > 0) {
		printf(".");
		fflush(stdout);
		usleep(step * 1000);
		ms -= step;
	}
	printf("\n");

	return ping(node);
}

static int slash_csp_switch(struct slash * slash) {
	if (slash->argc < 3)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	unsigned int slot = atoi(slash->argv[2]);

	unsigned int times = 1;
	if (slash->argc > 3) {
		times = atoi(slash->argv[3]);
	}

	int type = 0;
	if (slot >= 2)
		type = 1;
	return reset_to_flash(node, slot, times, type);
}

slash_command(switch, slash_csp_switch, "<node> <slot> [times]", "switch");

static vmem_list_t vmem_list_find(int node, int timeout, char * name, int namelen) {
	vmem_list_t ret = {};

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_NONE);
	if (conn == NULL)
		return ret;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	vmem_request_t * request = (void *)packet->data;
	request->version = 1;
	request->type = VMEM_SERVER_LIST;
	packet->length = sizeof(vmem_request_t);

	csp_send(conn, packet);

	/* Wait for response */
	packet = csp_read(conn, timeout);
	if (packet == NULL) {
		printf("No response\n");
		csp_close(conn);
		return ret;
	}

	for (vmem_list_t * vmem = (void *)packet->data; (intptr_t)vmem < (intptr_t)packet->data + packet->length; vmem++) {
		// printf(" %u: %-5.5s 0x%08X - %u typ %u\r\n", vmem->vmem_id, vmem->name, (unsigned int) be32toh(vmem->vaddr), (unsigned int) be32toh(vmem->size), vmem->type);
		if (strncmp(vmem->name, name, namelen) == 0) {
			ret.vmem_id = vmem->vmem_id;
			ret.type = vmem->type;
			memcpy(ret.name, vmem->name, 5);
			ret.vaddr = be32toh(vmem->vaddr);
			ret.size = be32toh(vmem->size);
		}
	}

	csp_buffer_free(packet);
	csp_close(conn);

	return ret;
}

static int image_get(char * filename, char ** data, int * len) {

	/* Open file */
	FILE * fd = fopen(filename, "r");
	if (fd == NULL) {
		printf("  Cannot find file: %s\n", filename);
		return -1;
	}

	/* Read size */
	struct stat file_stat;
	fstat(fd->_fileno, &file_stat);

	/* Copy to memory:
	 * Note we ignore the memory leak because the application will terminate immediately after using the data */
	*data = malloc(file_stat.st_size);
	*len = fread(*data, 1, file_stat.st_size, fd);
	fclose(fd);

	return 0;
}

#if 0
static void upload(int node, int address, char * data, int len) {

	unsigned int timeout = 10000;
	printf("  Upload %u bytes to node %u addr 0x%x\n", len, node, address);
	vmem_upload(node, timeout, address, data, len, 1);
	printf("  Waiting for flash driver to flush\n");
	usleep(100000);
}
#endif


#define BIN_PATH_MAX_ENTRIES 10
#define BIN_PATH_MAX_SIZE 100

struct bin_info_t {
	uint32_t addr_min;
	uint32_t addr_max;
	unsigned count;
	char entries[BIN_PATH_MAX_ENTRIES][BIN_PATH_MAX_SIZE];
} bin_info;

static char wpath[BIN_PATH_MAX_SIZE];

// Binary file byte offset of entry point address.
// C21: 4, E70: 2C4
static const uint32_t entry_offsets[] = { 4, 0x2c4 };

bool is_valid_binary(const char * path, struct bin_info_t * binf)
{
	int len = strlen(path);
	if ((len <= 4) || (strcmp(&(path[len-4]), ".bin") != 0)) {
		return false;
	}

	char * data;
	if (image_get((char*)path, &data, &len) < 0) {
		return false;
	}

	if (binf->addr_min + len <= binf->addr_max) {
		uint32_t addr = 0;
		for (size_t i = 0; i < sizeof(entry_offsets)/sizeof(uint32_t); i++) {
			addr = *((uint32_t *) &data[entry_offsets[i]]);
			if ((binf->addr_min <= addr) && (addr <= binf->addr_max)) {
				free(data);
				return true;
			}
		}
	}
	free(data);
	return false;
}

static bool dir_callback(const char * path, const char * last_entry, void * custom) 
{
	return true;
}

static void file_callback(const char * path, const char * last_entry, void * custom)
{
    struct bin_info_t * binf = (struct bin_info_t *)custom; 
	if (binf && is_valid_binary(path, binf)) {
		if (binf->count < BIN_PATH_MAX_ENTRIES) {
			strncpy(binf->entries[binf->count], path, BIN_PATH_MAX_SIZE);
			binf->count++;
		}
		else {
			printf("More than %u binaries found. Searched stopped.\n", BIN_PATH_MAX_ENTRIES);
		}
	}
}

static void walk_dir(char * path, size_t path_size, unsigned depth, 
					 bool (*dir_cb)(const char *, const char *, void *), 
					 void (*file_cb)(const char *, const char *, void *), 
					 void * custom)
{
    DIR * p = opendir(path);
	if (p == 0) {
		return;
	}
	
    struct dirent * entry;
	struct stat stat_info;

    while ((entry = readdir(p)) != NULL) {
		// Save path length
		size_t path_len = strlen(path);

		strncat(path, "/", path_size);
		strncat(path, entry->d_name, path_size);

		// entry->d_type is not set on some file systems, like sshfs mount of si
		lstat(path, &stat_info);
		bool isdir = S_ISDIR(stat_info.st_mode);
		bool isfile = S_ISREG(stat_info.st_mode);

  		// Ignore '.', '..' and hidden entries
        if (((isdir && depth) || isfile) && (entry->d_name[0] != '.')) {
        	if (isdir) {
				if (dir_cb && dir_cb(path, entry->d_name, custom)) {
					walk_dir(path, path_size, depth-1, dir_cb, file_cb, custom);
				}
			} 
			else {
				if (file_cb) {
					file_cb(path, entry->d_name, custom);
				}
			}
		}

		// Restore path length
		path[path_len] = 0;
    }
    closedir(p);
}

static int upload_and_verify(int node, int address, char * data, int len) {

	unsigned int timeout = 10000;
	printf("  Upload %u bytes to node %u addr 0x%x\n", len, node, address);
	vmem_upload(node, timeout, address, data, len, 1);

	char * datain = malloc(len);
	vmem_download(node, timeout, address, len, datain, 1);

	for (int i = 0; i < len; i++) {
		if (datain[i] == data[i])
			continue;
		printf("Diff at %x: %hhx != %hhx\n", address + i, data[i], datain[i]);
		free(datain);
		return SLASH_EINVAL;
	}

	free(datain);
	return SLASH_SUCCESS;
}

static int slash_csp_program(struct slash * slash) {
	if (!((slash->argc == 3) || (slash->argc == 4)))
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	unsigned int slot = atoi(slash->argv[2]);
	char * arg_path = (slash->argc >= 4) ? slash->argv[3] : 0;

	char vmem_name[5];
	snprintf(vmem_name, 5, "fl%u", slot);

	printf("  Requesting VMEM name: %s...\n", vmem_name);

	vmem_list_t vmem = vmem_list_find(node, 5000, vmem_name, strlen(vmem_name));
	if (vmem.size == 0) {
		printf("Failed to find vmem on subsystem\n");
		return SLASH_EINVAL;
	} else {
		printf("  Found vmem\n");
		printf("    Base address: 0x%x\n", vmem.vaddr);
		printf("    Size: %u\n", vmem.size);
	}

	if (arg_path) {
		strncpy(bin_info.entries[0], arg_path, BIN_PATH_MAX_SIZE);
		bin_info.count = 0;
	}
	else {
		printf("  Searching for valid binaries\n");
		strcpy(wpath, ".");
		bin_info.addr_min = vmem.vaddr;
		bin_info.addr_max = vmem.vaddr + vmem.size;
		bin_info.count = 0;
		walk_dir(wpath, BIN_PATH_MAX_SIZE, 10, dir_callback, file_callback, &bin_info);
		
		if (bin_info.count) {
			for (unsigned i = 0; i < bin_info.count; i++) {
				printf("  %u: %s\n", i, bin_info.entries[i]);
			}
		}
		else {
			printf("\033[31m\n");
			printf("  Found no valid binary for the selected slot.\n");
			printf("\033[0m\n");
			return SLASH_EINVAL;
		}
	}

	int index = 0;
	if (bin_info.count > 1) {
		char * c = slash_readline(slash, "Type number to select file: ");
		if (strlen(c) == 0) {
	        printf("Abort\n");
	        return SLASH_EUSAGE;
		}
		index = atoi(c);
	}
	
	char * path = bin_info.entries[index];

    printf("\033[31m\n");
    printf("ABOUT TO PROGRAM: %s\n", path);
    printf("\033[0m\n");
    if (ping(node) != SLASH_SUCCESS) {
		return SLASH_EINVAL;
	}
    printf("\n");

	char * c = slash_readline(slash, "Type 'yes' + enter to continue: ");
    
    if (strcmp(c, "yes") != 0) {
        printf("Abort\n");
        return SLASH_EUSAGE;
    }

	char * data;
	int len;
	if (image_get(path, &data, &len) < 0) {
		return SLASH_EIO;
	}
	return upload_and_verify(node, vmem.vaddr, data, len);
}

slash_command(program, slash_csp_program, "<node> <slot> [filename]", "program");