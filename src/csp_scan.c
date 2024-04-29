#include <stdio.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <csp/csp.h>
#include <csp/csp_cmp.h>

static int csp_scan(struct slash *slash)
{
    unsigned int begin = 0;
    unsigned int end = 0x3FFE;
	char * search_str = 0;

    optparse_t * parser __attribute__((cleanup(optparse_del))) = optparse_new("csp scan", NULL);
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'b', "begin", "NUM", 0, &begin, "begin at node");
    optparse_add_unsigned(parser, 'e', "end", "NUM", 0, &end, "end at node");
    optparse_add_string(parser, 's', "search", "STR", &search_str, "host name search sub-string");
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
	    return SLASH_EINVAL;
    }

	bool search = (search_str && (strlen(search_str) > 0));

	printf("CSP SCAN  [%u:%u]\n", begin, end);
	if (search) {
		printf("Searching for host name sub-string '%s'\n", search_str);
	}

    for (unsigned int i = begin; i <= end; i++) {
        printf("trying %u: \r", i);
        if (csp_ping(i, 20, 0, CSP_O_CRC32) >= 0) {
			printf("Found something on addr %u...\n", i);
				
			struct csp_cmp_message message;
			if (csp_cmp_ident(i, 100, &message) == CSP_ERR_NONE) {
				if ((!search) || (strstr(message.ident.hostname, search_str) != NULL)) {
					printf("%s\n%s\n%s\n%s %s\n\n", message.ident.hostname, message.ident.model, message.ident.revision, message.ident.date, message.ident.time);
				}
			}
        }

        /* Check for exit command */
		if (slash_wait_interruptible(slash, 0) != 0)
			break;
    }
	printf("\r");
    return SLASH_SUCCESS;
}

slash_command_sub(csp, scan, csp_scan, NULL, NULL);
