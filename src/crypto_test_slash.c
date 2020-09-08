/*
 * vmem_client_slash.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */


#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>

#include <slash/slash.h>

#include "crypto_test.h"

static int crypto_test_send(struct slash *slash)
{
    int node = csp_get_address();
    int timeout = 2000;
    char * endptr;

    if (slash->argc >= 2) {
        node = strtoul(slash->argv[1], &endptr, 10);
        if (*endptr != '\0')
            return SLASH_EUSAGE;
    }

    if (slash->argc >= 3) {
        timeout = strtoul(slash->argv[2], &endptr, 10);
        if (*endptr != '\0')
            return SLASH_EUSAGE;
    }

    printf("Sending encrypted packed to %u timeout %u\n", node, timeout);

    char test[] = "Hello this is a test message!!";
    crypto_test_echo(1, (uint8_t*)test, sizeof(test));

    return SLASH_SUCCESS;
}

slash_command_sub(crypto, send, crypto_test_send, "<node> <timeout>", NULL);
