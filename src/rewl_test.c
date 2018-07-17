/*
 * rewl_test.c
 *
 *  Created on: Apr 3, 2018
 *      Author: johan
 */

#include <stdio.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_queue.h>
#include <param/param_client.h>
#include <slash/slash.h>

static char rewl_pull_buf[25];
static param_queue_t rewl_pull_q = {
	.buffer = rewl_pull_buf,
	.buffer_size = 25,
	.type = PARAM_QUEUE_TYPE_GET,
	.used = 0,
};

static int rewl_log_slash(struct slash *slash) {
	param_t * rpm = param_list_find_id(0, 140);
	param_queue_add(&rewl_pull_q, rpm, 0, NULL);
	float t = 0;
	printf("Logging REWL Data, press any key to interrupt\n");
	while(1) {

		param_pull_queue(&rewl_pull_q, 0, 0, 100);

		/* Delay (press enter to exit) */
		if (slash_wait_interruptible(slash, 100) != 0)
			break;

		printf("%f, %f\n", t, param_get_float(rpm));
		t += 0.1;

	};
	return SLASH_SUCCESS;
}

slash_command(rewl_log, rewl_log_slash, NULL, "Log REWL data");
