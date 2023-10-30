/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Space Inventor ApS
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>

static int slash_crc32_verify(struct slash *slash)
{

	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;
	char *filename = NULL;
	uint64_t vmem_addr = 0;

	optparse_t * parser = optparse_new("verify", "<filename> <address>");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("missing parameter <filename>\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	filename = strdup(slash->argv[argi]);

	char *endptr = NULL;
	if (++argi >= slash->argc) {
		printf("missing parameter <address>\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (!strncmp(slash->argv[argi],"0x",2)) {
		uint32_t arg_len = strlen(slash->argv[argi]) - 2;
		bool bad_hex_char = false;
		for (uint32_t n=0; n<arg_len; n++) {
			if (!strpbrk(&slash->argv[argi][n+2], "0123456789abcdefABCDEF")) {
				bad_hex_char = true;
				break;
			}
		}
		if (!bad_hex_char) {
			vmem_addr = strtoull(slash->argv[argi],&endptr,16);
		} else {
			printf("invalid <address> parameter\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}
	} else if (strpbrk(slash->argv[argi], "0123456789")) {
		vmem_addr = strtoull(slash->argv[argi],&endptr,10);
	} else {
		printf("invalid <address> parameter\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	slash_printf(slash, "File: %s, Addr:0x%"PRIX64" (%"PRIu64")\n", filename, vmem_addr, vmem_addr);

	optparse_del(parser);
	return SLASH_SUCCESS;

}

static void slash_crc32_verify_completer(struct slash *slash, char * token) {

	printf("Token: %s,%d,%d\n", token, slash->cursor, slash->argc);

}

//slash_command_sub(crc32, verify, slash_crc32_verify, "<filename> <VMEM address>", "Verify the crc32 checksum between a file and a VMEM area");
slash_command_sub_completer(crc32, verify, slash_crc32_verify, slash_crc32_verify_completer, "<filename> <VMEM address>", "Verify the crc32 checksum between a file and a VMEM area");
