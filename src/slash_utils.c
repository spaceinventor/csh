#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#include <slash/slash.h>
#include <apm/csh_api.h>

optparse_opt_t *csh_add_node_option(optparse_t * parser, unsigned int *node) {
	static char help[64];
	*node = slash_dfl_node;
	snprintf(help, sizeof(help), "node address in decimal or name (default = %d)", *node);
	return optparse_add_custom(parser,
		'n', "node", "NUM", help,
		get_host_by_addr_or_name, node);
}

int parse_vmem_address(struct slash *slash, int *argi, uint64_t *address) {

	if (++(*argi) >= slash->argc) {
		printf("missing address\n");
		return SLASH_EUSAGE;
	}

	char * endptr;
	*address = strtoul(slash->argv[*argi], &endptr, 16);
	if (*endptr != '\0') {
		printf("Failed to parse address\n");
		return SLASH_EINVAL;
	}

	return SLASH_SUCCESS;
}

int parse_length(struct slash *slash, int *argi, uint32_t *length) {

	if (++(*argi) >= slash->argc) {
		printf("missing length\n");
		return SLASH_EINVAL;
	}

	char * endptr;
	uint64_t length64;
	length64 = strtoul(slash->argv[*argi], &endptr, 10);
	if (*endptr != '\0') {
		length64 = strtoul(slash->argv[*argi], &endptr, 16);
		if (*endptr != '\0' || strncmp(slash->argv[*argi], "0x", 2) != 0) {
			printf("Failed to parse length in base 10 or base 16\n");
			return SLASH_EUSAGE;
		}

	}

	if (length64 > UINT32_MAX) {
		printf("Length is too large\n");
		return SLASH_EINVAL;
	}

	*length = (uint32_t)length64;

	return SLASH_SUCCESS;
}
