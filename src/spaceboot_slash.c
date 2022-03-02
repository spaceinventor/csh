

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

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

static void ping(int node) {

	struct csp_cmp_message message = {};
	if (csp_cmp_ident(node, 1000, &message) != CSP_ERR_NONE) {
		printf("Cannot ping system\n");
		exit(EXIT_FAILURE);
	}
	printf("  | %s\n  | %s\n  | %s\n  | %s %s\n", message.ident.hostname, message.ident.model, message.ident.revision, message.ident.date, message.ident.time);
}

static void reset_to_flash(int node, int flash, int times, int type) {

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

	ping(node);
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
	reset_to_flash(node, slot, times, type);

	return SLASH_SUCCESS;
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

	printf("  Open %s size %zu\n", filename, file_stat.st_size);

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
	if (slash->argc < 4)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	unsigned int slot = atoi(slash->argv[2]);
	char * path = slash->argv[3];

	char * data;
	int len;
	if (image_get(path, &data, &len) < 0) {
		return SLASH_EIO;
	}

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

	if (len > vmem.size) {
		printf("Software image too large for vmem\n");
		return SLASH_EINVAL;
	}

    uint32_t mainaddr = *((uint32_t *) &data[4]);
    
    if ((mainaddr < vmem.vaddr) || (mainaddr > vmem.vaddr + vmem.size)) {
        printf("  Main from %s is outside %s at 0x%x\n", path, vmem.name, mainaddr);
        return SLASH_EINVAL;
    }

    printf("\033[31m\n");
    printf("ABOUT TO PROGRAM:\n");
    printf("\033[0m\n");
    ping(node);
    printf("\n");

    printf("\033[32m\n");
    printf("  Main from %s is within %s at 0x%x\n", path, vmem.name, mainaddr);
    printf("\033[0m\n");

	char *c = slash_readline(slash, "Type 'yes' + enter to continue: ");
    
    if (strcmp(c, "yes") != 0) {
        printf("Abort\n");
        return SLASH_EUSAGE;
    }

    return upload_and_verify(node, vmem.vaddr, data, len);
}

slash_command(program, slash_csp_program, "<node> <slot> <filename> ", "program");