#include <stdint.h>
#include <stdlib.h>
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

