/*
 * param_scheduler_slash.c
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <apm/csh_api.h>

#include <csp/csp.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include "param_scheduler_client.h"

extern param_queue_t param_queue;

static int cmd_schedule_push(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;
	unsigned int host = slash_dfl_node;
	unsigned int latency_buffer = 0;

    optparse_t * parser = optparse_new("schedule push", "<time>");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_custom(parser, 's', "server", "NUM", "server to push parameters to (default = <env>))", get_host_by_addr_or_name, &server);
	optparse_add_custom(parser, 'H', "host", "NUM", "host to receive push queue (default = queue host))", get_host_by_addr_or_name, &host);
	optparse_add_unsigned(parser, 'l', "latency", "NUM", 0, &latency_buffer, "max latency, 0 to disable (default = 0))");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("Must specify relative or absolute time in seconds\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	unsigned int sch_time = atoi(slash->argv[argi]);
	long sch_time_long = (long) sch_time;
	struct tm * sch_datetime = gmtime(&sch_time_long);
	
	time_t current_time = time(NULL);
	char timestr[32] ={0};
	strftime(timestr, sizeof(timestr)-1, "%F T%TZ%z", sch_datetime);

	if (sch_time+latency_buffer-timeout < current_time && sch_time >= 1E9) {
		printf("\033[0;31mScheduled time is: %s (%u) and is behind current time\033[0m\n", timestr, sch_time);
		printf("Confirm: Type 'yes' or 'y' + enter to push schedule:\n");
		char * c = slash_readline(slash);
		if (strcasecmp(c, "yes") != 0 && strcasecmp(c, "y") != 0) {
			printf("\033[0;31mSchedule not pushed\033[0m\n");
			optparse_del(parser);
			return SLASH_EBREAK;
		}
	}

	if (sch_time < 1E9) {
		sch_time_long += current_time;
		sch_datetime = gmtime(&sch_time_long);
		strftime(timestr, sizeof(timestr)-1, "%F T%TZ%z", sch_datetime);
		printf("Pushing schedule for approx. time: %s (%ld)\n", timestr, sch_time_long);
	} else {
		printf("Pushing schedule for time: %s (%u)\n", timestr, sch_time);
	}
	
	if (param_schedule_push(&param_queue, 1, server, host, sch_time, latency_buffer, timeout) < 0) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EIO;
	}

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(schedule, push, cmd_schedule_push, "", NULL);

static int cmd_schedule_list(struct slash *slash) {

    unsigned int server = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("schedule list", "");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_custom(parser, 's', "server", "NUM", "server to push parameters to (default = <env>))", get_host_by_addr_or_name, &server);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (param_list_schedule(server, 1, timeout) < 0) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EIO;
	}

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(schedule, list, cmd_schedule_list, "", NULL);

static int cmd_schedule_show(struct slash *slash) {

    unsigned int server = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("schedule show", "<id>");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_custom(parser, 's', "server", "NUM", "server to push parameters to (default = <env>))", get_host_by_addr_or_name, &server);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("Must specify schedule ID\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
	unsigned int id = atoi(slash->argv[argi]);

	if (param_show_schedule(server, 1, id, timeout) < 0) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EIO;
	}

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(schedule, show, cmd_schedule_show, "", NULL);

static int cmd_schedule_rm(struct slash *slash) {

    unsigned int server = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;
	int rm_all = 0;

    optparse_t * parser = optparse_new("schedule rm", "<id>");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_set(parser, 'a', "all", 1, &rm_all, "delete all");
	optparse_add_custom(parser, 's', "server", "NUM", "server to push parameters to (default = <env>))", get_host_by_addr_or_name, &server);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	unsigned int id = 0;

	if (!rm_all) {
		if (++argi >= slash->argc) {
			printf("Must specify schedule ID\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}
		id = atoi(slash->argv[argi]);

		if (id < 0)
			return SLASH_EUSAGE;
	}

	if (rm_all) {
		if (param_rm_all_schedule(server, 1, timeout) < 0) {
			printf("No response\n");
	        optparse_del(parser);
			return SLASH_EIO;
		}
	} else {
		if (param_rm_schedule(server, 1, id, timeout) < 0) {
			printf("No response\n");
	        optparse_del(parser);
			return SLASH_EIO;
		}
	}

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(schedule, rm, cmd_schedule_rm, "<server> <id> [timeout]", NULL);

static int cmd_schedule_reset(struct slash *slash) {

    unsigned int server = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("schedule list", "");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_custom(parser, 's', "server", "NUM", "server to push parameters to (default = <env>))", get_host_by_addr_or_name, &server);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("Must specify schedule ID\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	uint16_t last_id = atoi(slash->argv[argi]);

	if (param_reset_scheduler(server, last_id, 1, timeout) < 0) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EIO;
	}

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(schedule, reset, cmd_schedule_reset, "<server> <last id> [timeout]", NULL);

static void parse_name(char out[], char in[]) {
	for (int i = 0; i < strlen(in); i++) {
			out[i] = in[i];
		}
	out[strlen(in)] = '\0';
}

static int cmd_schedule_command(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;
	unsigned int host = slash_dfl_node;
	unsigned int latency_buffer = 0;

    optparse_t * parser = optparse_new("schedule cmd", "<name> <time>");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_custom(parser, 's', "server", "NUM", "server to push parameters to (default = <env>))", get_host_by_addr_or_name, &server);
	optparse_add_custom(parser, 'H', "host", "NUM", "host to receive push queue (default = queue host))", get_host_by_addr_or_name, &host);
	optparse_add_unsigned(parser, 'l', "latency", "NUM", 0, &latency_buffer, "max latency, 0 to disable (default = 0))");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	char name[14] = {0};
	if (++argi >= slash->argc) {
		printf("Must command name to schedule\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (strlen(slash->argv[argi]) >= sizeof(name)) {
		optparse_del(parser);
		return SLASH_EUSAGE;
	}
	parse_name(name, slash->argv[argi]);

	if (++argi >= slash->argc) {
		printf("Must specify relative or absolute time in seconds\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
	unsigned int sch_time = atoi(slash->argv[argi]);

	long sch_time_long = (long) sch_time;
	struct tm * sch_datetime = gmtime(&sch_time_long);
	
	time_t current_time = time(NULL);
	char timestr[32] ={0};
	strftime(timestr, sizeof(timestr)-1, "%F T%TZ%z", sch_datetime);

	if (sch_time+latency_buffer-timeout < current_time && sch_time >= 1E9) {
		printf("\033[0;31mScheduled time is: %s (%u) and is behind current time\033[0m\n", timestr, sch_time);
		printf("Confirm: Type 'yes' or 'y' + enter to schedule anyway:\n");
		char * c = slash_readline(slash);
		if (strcasecmp(c, "yes") != 0 && strcasecmp(c, "y") != 0) {
			printf("\033[0;31mSchedule not pushed\033[0m\n");
			optparse_del(parser);
			return SLASH_EBREAK;
		}
	}

	if (sch_time < 1E9) {
		sch_time_long += current_time;
		sch_datetime = gmtime(&sch_time_long);
		strftime(timestr, sizeof(timestr)-1, "%F T%TZ%z", sch_datetime);
		printf("Pushing schedule for approx. time: %s (%ld)\n", timestr, sch_time_long);
	} else {
		printf("Pushing schedule for time: %s (%u)\n", timestr, sch_time);
	}

	if (param_schedule_command(1, server, name, host, sch_time, latency_buffer, timeout) < 0) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EIO;
	}

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(schedule, cmd, cmd_schedule_command, "", NULL);
