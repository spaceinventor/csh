
#include "walkdir.h"

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <apm/csh_api.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>

#include <vmem/vmem.h>
#include <vmem/vmem_client.h>
#include <vmem/vmem_server.h>
#include <vmem/vmem_ram.h>

#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <csp/csp_crc32.h>
#include <csp/arch/csp_time.h>

#include "csh_internals.h"

static int ping(int node) {

	struct csp_cmp_message message = {};
	if (csp_cmp_ident(node, 3000, &message) != CSP_ERR_NONE) {
		printf("Cannot ping system\n");
		return 0;
	}
	printf("  | %s\n  | %s\n  | %s\n  | %s %s\n", message.ident.hostname, message.ident.model, message.ident.revision, message.ident.date, message.ident.time);
	return 1;
}

static void reset_to_flash(int node, int flash, int times, int ms) {

#define NUM_SLOTS 4

	param_t * boot_img[NUM_SLOTS];
	bool boot_img_exist[NUM_SLOTS];
	int param_id[NUM_SLOTS] = {21, 20, 22, 23};
	char param_name[NUM_SLOTS][10];
	/* Setup remote parameters */
	for (int i = 0; i < NUM_SLOTS; i++) {
		boot_img[i] = param_list_find_id(node, param_id[i]);
		if (boot_img[i]) {
			boot_img_exist[i] = 1;
		} else {
			snprintf(param_name[i], sizeof(param_name[i]), "boot_img%u", i);
			boot_img[i] = param_list_create_remote(param_id[i], node, PARAM_TYPE_UINT8, PM_CONF, 0, param_name[i], NULL, NULL, -1);
			boot_img_exist[i] = param_list_add(boot_img[i]);
		}
	}

	printf("  Switching to flash %d\n", flash);
	printf("  Will run this image %d times\n", times);

	char queue_buf[50];
	param_queue_t queue;
	param_queue_init(&queue, queue_buf, 50, 0, PARAM_QUEUE_TYPE_SET, 2);

	uint8_t zero = 0;
	for (int i = 0; i < NUM_SLOTS; i++) {
		param_queue_add(&queue, boot_img[i], 0, &zero);
	}
	param_queue_add(&queue, boot_img[flash], 0, &times);
	param_push_queue(&queue, CSP_PRIO_NORM, 1, node, 1000, 0, false);

	printf("  Rebooting");
	csp_reboot(node);
	int step = 25;
	while (ms > 0) {
		printf(".");
		fflush(stdout);
		usleep(step * 1000);
		ms -= step;
	}
	printf("\n");

	for (int i = 0; i < NUM_SLOTS; i++) {
		if (!boot_img_exist[i]) param_list_remove_specific(boot_img[i], false, true);
	}

	ping(node);
}

static int slash_csp_switch(struct slash * slash) {


	unsigned int node = slash_dfl_node;
	unsigned int times = 1;
	unsigned int reboot_delay = 1000;

    optparse_t * parser = optparse_new("switch", "<slot>");
    optparse_add_help(parser);
    csh_add_node_option(parser, &node);
    optparse_add_unsigned(parser, 'c', "count", "NUM", 0, &times, "number of times to boot into this slow (deafult = 1)");
	optparse_add_unsigned(parser, 'd', "delay", "NUM", 0, &reboot_delay, "Delay to allow module to boot (default = 1000 ms)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Expect slot */
	if (++argi >= slash->argc) {
		printf("missing slot number\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	unsigned int slot = atoi(slash->argv[argi]);

	reset_to_flash(node, slot, times, reboot_delay);

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command(switch, slash_csp_switch, "<slot>", "switch");

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

typedef struct bin_file_ident_s {
	bool valid;
	char hostname[32+1];
	char model[32+1];
	char version_string[32+1];
	uint32_t stext;
} bin_file_ident_t;

struct bin_info_t {
	uint32_t addr_min;
	uint32_t addr_max;
	unsigned count;
	bin_file_ident_t idents[WALKDIR_MAX_ENTRIES];
	char files[WALKDIR_MAX_ENTRIES][WALKDIR_MAX_PATH_SIZE];
} bin_info;

static char wpath[WALKDIR_MAX_PATH_SIZE];

// Binary file byte offset of entry point address.
// C21: 4, E70: 2C4
static const uint32_t entry_offsets[] = { 4, 0x2c4 };

bool is_valid_binary(const char * path, struct bin_info_t * binf, bin_file_ident_t * binf_ident)
{
	binf_ident->valid = false;

	/* 1. does the file have the .bin extention */
	int len = strlen(path);
	if ((len <= 4) || (strcmp(&(path[len-4]), ".bin") != 0)) {
		return false;
	}

	/* 2. read all the data from the file */
	char * data;
	if (image_get((char*)path, &data, &len) < 0) {
		return false;
	}

	/* 3. detect if there is a IDENT structure at the end */
	bool ident_found = false;
	char *ident_str[3] = { &binf_ident->hostname[0], &binf_ident->model[0], &binf_ident->version_string[0] };
	int32_t idx = -4;
	if (!memcmp(&data[len + idx], "\xC0\xDE\xBA\xD0", 4)) {
		idx -= 4;

		/* 3.1. grab the stext address (start of text) */
		binf_ident->stext = ((uint32_t *)(&data[len + idx]))[0];

		/* 3.2. verify that the entry lies within the vmem area to be programmed */
		if ((binf->addr_min <= binf_ident->stext) && (binf->addr_max >= binf_ident->stext) && ((binf->addr_min + len) <= binf->addr_max)) {
			/* 3.2.1. scan for an other magic marker */
			char *ident_begin = NULL;
			do {
				idx--;
				if (!memcmp(&data[len + idx], "\xBA\xD0\xFA\xCE", 4)) {
					ident_begin = &data[len + idx + 4];
					break;
				}
			} while (idx >= -256);
			/* 3.2.2. if we found the beginning, we can extract IDENT strings */
			char *ident_iter;
			uint8_t ident_id;
			for (ident_iter = ident_begin, ident_id = 0; ident_iter && (ident_iter < &data[len - 4] && ident_id < 3); ident_id++) {
				if (ident_iter) {
					strncpy(ident_str[ident_id], ident_iter, 32);
				}
				ident_iter += (strlen(ident_iter) + 1);
				ident_found = true;
				binf_ident->valid = true;
			}
		} else {
			/* We found the magic marker and the entry point address, but it did not match the area */
			free(data);
			return false;
		}
	}

	if (!ident_found) {
		/* 4. analyze the "magic position" for a valid value - might be the Reset_Handler address in the vector table */
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
	}

	free(data);
	return ident_found;
}

static bool dir_callback(const char * path, const char * last_entry, void * custom)
{
	return true;
}

static void file_callback(const char * path, const char * last_entry, void * custom)
{
    struct bin_info_t * binf = (struct bin_info_t *)custom;
	bin_file_ident_t binf_ident;
	if (binf && is_valid_binary(path, binf, &binf_ident)) {
		if (binf->count < WALKDIR_MAX_ENTRIES) {
			binf->idents[binf->count] = binf_ident;
			strncpy(&binf->files[binf->count][0], path, WALKDIR_MAX_PATH_SIZE);
			binf->count++;
		}
		else {
			printf("More than %u binaries found. Searched stopped.\n", WALKDIR_MAX_ENTRIES);
		}
	}
}

static int do_upload(int node, unsigned int timeout, int address, char * data, int len) {
	int res = SLASH_SUCCESS;
	uint32_t time_begin = csp_get_ms();
	int count = vmem_upload(node, timeout, address, data, len, 1);
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
			if(count != len){
				unsigned int window_size = 0;
				csp_rdp_get_opt(&window_size, NULL, NULL, NULL, NULL, NULL);
				uint32_t suggested_offset = 0;
				if(count > (((signed int)window_size + 1) * VMEM_SERVER_MTU)) {
					suggested_offset = (count) - ((window_size + 1) * VMEM_SERVER_MTU);
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
	return res;
}

static int upload_and_verify(int node, int address, char * data, int len) {

	unsigned int timeout = 10000;
	printf("  Upload %u bytes to node %u addr 0x%x\n", len, node, address);
	int res = do_upload(node, timeout, address, data, len);
	if(res < 0){
		return res;
	}

	char * datain = malloc(len);
	vmem_download(node, timeout, address, len, datain, 1, 1);

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

	unsigned int node = slash_dfl_node;
	char * filename = NULL;
	int force = 0;
	int do_crc32 = 0;

    optparse_t * parser = optparse_new("program", "<slot>");
    optparse_add_help(parser);
    csh_add_node_option(parser, &node);
    optparse_add_string(parser, 'f', "file", "FILENAME", &filename, "File to upload (Searches recursively by default)");
    optparse_add_set(parser, 'F', "force", 1, &force, "Do not ask for confirmation before programming");
    optparse_add_set(parser, 'C', "crc32", 1, &do_crc32, "Compare CRC32 as a program success criteria");

	rdp_opt_add(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Expect slot */
	if (++argi >= slash->argc) {
		printf("missing slot number\n");
		rdp_opt_reset();
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	unsigned int slot = atoi(slash->argv[argi]);

	char vmem_name[5];
	snprintf(vmem_name, 5, "fl%u", slot);

	printf("  Requesting VMEM name: %s...\n", vmem_name);

	vmem_list_t vmem = {0};
	vmem_client_find(node, slash_dfl_timeout, (void*)&vmem, 1, vmem_name, strlen(vmem_name));
	if (vmem.size == 0) {
		printf("Failed to find vmem on subsystem\n");
		rdp_opt_reset();
        optparse_del(parser);
		return SLASH_EINVAL;
	} else {
		printf("  Found vmem\n");
		printf("    Base address: 0x%x\n", vmem.vaddr);
		printf("    Size: %u\n", vmem.size);
	}

	if (filename) {
		strncpy(bin_info.files[0], filename, WALKDIR_MAX_PATH_SIZE-1);  // -1 to fit NULL byte
		bin_info.count = 0;
	} else {
		printf("  Searching for valid binaries\n");
		strcpy(wpath, ".");
		bin_info.addr_min = vmem.vaddr;
		bin_info.addr_max = (vmem.vaddr + vmem.size) - 1;
		bin_info.count = 0;
		walkdir(wpath, WALKDIR_MAX_PATH_SIZE - 10, 10, dir_callback, file_callback, &bin_info, &slash->signal);
		if(slash->signal == SIGINT){
            optparse_del(parser);
			rdp_opt_reset();
			return SLASH_EINVAL;
		}
		if (bin_info.count) {
 			for (unsigned i = 0; i < bin_info.count; i++) {
				if (bin_info.idents[i].valid) {
					printf("  %u: %s (%s, %s, %s, 0x%08"PRIX32")\n", i, &bin_info.files[i][0], bin_info.idents[i].hostname, bin_info.idents[i].model, bin_info.idents[i].version_string, bin_info.idents[i].stext);
				} else {
					printf("  %u: %s\n", i, &bin_info.files[i][0]);
				}
			}
		}
		else {
			printf("\033[31m\n");
			printf("  Found no valid binary for the selected slot.\n");
			printf("\033[0m\n");

            optparse_del(parser);
			rdp_opt_reset();
			return SLASH_EINVAL;
		}
	}

	int index = 0;
	if (bin_info.count > 1) {
		printf("Type number to select file:\n");
		char * c = slash_readline(slash);
		if (strlen(c) == 0) {
	        printf("Abort\n");
            optparse_del(parser);
			rdp_opt_reset();
	        return SLASH_EUSAGE;
		}
		index = atoi(c);
	}

	char * path = bin_info.files[index];

    printf("\033[31m\n");
    printf("ABOUT TO PROGRAM: %s\n", path);
    printf("\033[0m\n");
    if (ping(node) == 0) {
        optparse_del(parser);
		rdp_opt_reset();
		return SLASH_EINVAL;
	}
    printf("\n");

	if (!force) {
		printf("Type 'yes' + enter to continue:\n");
		char * c = slash_readline(slash);

		if (strcmp(c, "yes") != 0) {
			printf("Abort\n");
			optparse_del(parser);
			rdp_opt_reset();
			return SLASH_EUSAGE;
		}
	}

	char * data;
	int len;
	if (image_get(path, &data, &len) < 0) {
        optparse_del(parser);
		rdp_opt_reset();
		return SLASH_EIO;
	}

    optparse_del(parser);

	int result = SLASH_SUCCESS;

	if (do_crc32) {
		uint32_t crc;
		crc = csp_crc32_memory((const uint8_t *)data, len);
		printf("  File CRC32: 0x%08"PRIX32"\n", crc);
		printf("  Upload %u bytes to node %u addr 0x%"PRIX32"\n", len, node, vmem.vaddr);
		result = do_upload(node, 10000, vmem.vaddr, data, len);
		if(result < 0){
			return result;
		}
		uint32_t crc_node;
		int res = vmem_client_calc_crc32(node, 10000, vmem.vaddr, len, &crc_node, 1);
		if (res >= 0) {
			if (crc_node == crc) {
				printf("\033[32m\n");
				printf("  Success\n");
				printf("\033[0m\n");
			} else {
				printf("\033[31m\n");
				printf("  Failure: %"PRIX32" != %"PRIX32"\n", crc, crc_node);
				printf("\033[0m\n");
				result = SLASH_ENOSPC;
			}
		} else {
			printf("\033[31m\n");
			printf("  Communication failure: %"PRId32"\n", res);
			printf("\033[0m\n");
			result = SLASH_ENOSPC;
		}
	} else {
		result = upload_and_verify(node, vmem.vaddr, data, len);
	}

	free(data);

	rdp_opt_reset();

	return result;
}

slash_command(program, slash_csp_program, "<slot>", "Upload new firmware");


static int slash_sps(struct slash * slash) {

	unsigned int node = slash_dfl_node;
	unsigned int reboot_delay = 1000;
	int do_crc32 = 0;
	char * filename = NULL;

    optparse_t * parser = optparse_new("sps", "<switch-to-slot> <slot-to-program>");
    optparse_add_help(parser);
    csh_add_node_option(parser, &node);
	optparse_add_string(parser, 'f', "file", "FILENAME", &filename, "File to upload (Searches recursively by default)");
	optparse_add_unsigned(parser, 'd', "delay", "NUM", 0, &reboot_delay, "Delay to allow module to boot (default = 1000 ms)");
    optparse_add_set(parser, 'C', "crc32", 1, &do_crc32, "Compare CRC32 as a program success criteria");

	rdp_opt_add(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        rdp_opt_reset();
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	rdp_opt_set();

	/* Expect from slot */
	if (++argi >= slash->argc) {
		printf("missing from number\n");
        optparse_del(parser);
        rdp_opt_reset();
		return SLASH_EINVAL;
	}

	unsigned int from = atoi(slash->argv[argi]);

	/* Expect to slot */
	if (++argi >= slash->argc) {
		printf("missing to number\n");
        optparse_del(parser);
        rdp_opt_reset();
		return SLASH_EINVAL;
	}

	unsigned int to = atoi(slash->argv[argi]);

	reset_to_flash(node, from, 1, reboot_delay);

	char vmem_name[5];
	snprintf(vmem_name, 5, "fl%u", to);
	printf("  Requesting VMEM name: %s...\n", vmem_name);

	vmem_list_t vmem = {0};
	vmem_client_find(node, slash_dfl_timeout, (void*)&vmem, 1, vmem_name, strlen(vmem_name));
	if (vmem.size == 0) {
		printf("Failed to find vmem on subsystem\n");
        optparse_del(parser);
        rdp_opt_reset();
		return SLASH_EINVAL;
	} else {
		printf("  Found vmem\n");
		printf("    Base address: 0x%x\n", vmem.vaddr);
		printf("    Size: %u\n", vmem.size);
	}

    if (filename) {
        strncpy(bin_info.files[0], filename, WALKDIR_MAX_PATH_SIZE-1);  // -1 to fit NULL byte
        bin_info.count = 0;
    } else {

        printf("  Searching for valid binaries\n");
        strcpy(wpath, ".");
        bin_info.addr_min = vmem.vaddr;
        bin_info.addr_max = (vmem.vaddr + vmem.size) - 1;
        bin_info.count = 0;
        walkdir(wpath, WALKDIR_MAX_PATH_SIZE, 10, dir_callback, file_callback, &bin_info, &slash->signal);
        if(slash->signal == SIGINT){
            optparse_del(parser);
            rdp_opt_reset();
            return SLASH_EINVAL;
        }
        if (bin_info.count) {
            for (unsigned i = 0; i < bin_info.count; i++) {
                if (bin_info.idents[i].valid) {
                    printf("  %u: %s (%s, %s, %s, 0x%08"PRIX32")\n", i, &bin_info.files[i][0], bin_info.idents[i].hostname, bin_info.idents[i].model, bin_info.idents[i].version_string, bin_info.idents[i].stext);
                } else {
                    printf("  %u: %s\n", i, &bin_info.files[i][0]);
                }
            }
        }
        else {
            printf("\033[31m\n");
            printf("  Found no valid binary for the selected slot.\n");
            printf("\033[0m\n");
            optparse_del(parser);
            rdp_opt_reset();
            return SLASH_EINVAL;
        }

    }

	int index = 0;
	if (bin_info.count > 1) {
		printf("Type number to select file: ");
		char * c = slash_readline(slash);
		if (strlen(c) == 0) {
	        printf("Abort\n");
            optparse_del(parser);
            rdp_opt_reset();
	        return SLASH_EUSAGE;
		}
		index = atoi(c);
	}

	char * path = bin_info.files[index];

    printf("\033[31m\n");
    printf("ABOUT TO PROGRAM: %s\n", path);
    printf("\033[0m\n");
    if (ping(node) == 0) {
        optparse_del(parser);
        rdp_opt_reset();
		return SLASH_EINVAL;
	}
    printf("\n");

	char * data;
	int len;
	if (image_get(path, &data, &len) < 0) {
        optparse_del(parser);
        rdp_opt_reset();
		return SLASH_EIO;
	}

    optparse_del(parser);

	int result = SLASH_SUCCESS;

	if (do_crc32) {
		uint32_t crc;
		crc = csp_crc32_memory((const uint8_t *)data, len);
		printf("  File CRC32: 0x%08"PRIX32"\n", crc);
		printf("  Upload %u bytes to node %u addr 0x%"PRIX32"\n", len, node, vmem.vaddr);
		result = do_upload(node, 10000, vmem.vaddr, data, len);
		if(result < 0){
			return result;
		}

		uint32_t crc_node;
		int res = vmem_client_calc_crc32(node, 10000, vmem.vaddr, len, &crc_node, 1);
		if (res >= 0) {
			if (crc_node == crc) {
				printf("\033[32m\n");
				printf("  Success\n");
				printf("\033[0m\n");
			} else {
				printf("\033[31m\n");
				printf("  Failure: %"PRIX32" != %"PRIX32"\n", crc, crc_node);
				printf("\033[0m\n");
				result = SLASH_ENOSPC;
			}
		} else {
			printf("\033[31m\n");
			printf("  Communication failure: %"PRId32"\n", res);
			printf("\033[0m\n");
			result = SLASH_ENOSPC;
		}
	} else {
		result = upload_and_verify(node, vmem.vaddr, data, len);
	}

	if (result == SLASH_SUCCESS) {
		reset_to_flash(node, to, 1, reboot_delay);
	}

	free(data);

	rdp_opt_reset();

	return result;
}

slash_command(sps, slash_sps, "<node> <from> <to> [filename]", "switch program switch");