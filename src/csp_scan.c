#include <stdio.h>

#include <slash/slash.h>
#include <csp/csp.h>
#include <csp/csp_cmp.h>

static int csp_scan(struct slash *slash)
{
	printf("CSP SCAN\n");

    for (int i = 0; i < 0x3FFF; i++) {
        printf("\rtrying %d: ", i);
        if (csp_ping(i, 20, 0, CSP_O_CRC32) >= 0) {
            printf("\nFound something on addr %d...\n", i);
            
            struct csp_cmp_message message;
            if (csp_cmp_ident(i, 100, &message) == CSP_ERR_NONE) {
                printf("%s\n%s\n%s\n%s %s\n", message.ident.hostname, message.ident.model, message.ident.revision, message.ident.date, message.ident.time);
            }

        }

        /* Check for exit command */
		if (slash_wait_interruptible(slash, 0) != 0)
			break;
        
    }
    return SLASH_SUCCESS;
}

slash_command_sub(csp, scan, csp_scan, NULL, NULL);
