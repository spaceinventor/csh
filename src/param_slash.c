/*
 * param_slash.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/completer.h>
#include <apm/csh_api.h>

#include <csp/csp.h>
#include <csp/csp_hooks.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <param/param_string.h>

#include <param/param_wildcard.h>
#include "time.h"

static char queue_buf[PARAM_SERVER_MTU];
param_queue_t param_queue = { .buffer = queue_buf, .buffer_size = PARAM_SERVER_MTU, .type = PARAM_QUEUE_TYPE_EMPTY, .version = 2 };

int param_slash_parse_slice(char * token, int *start_index, int *end_index, int *slice_detected) {
	/**
	 * Function to find offsets and check if slice delimitor is active.
	 * @return Returns an array of three values. Defaults to INT_MIN if no value.
	 * @return Last value is 1 if found.
 */

	/* Search for the '@' symbol:
	* Call strtok twice in order to skip the stuff head of '@' */

	char _slice_delimitor;

	int first_scan = 0;
	int second_scan = 0;

	if (token != NULL) {
		// Searches for the format [digit:digit] and [digit:].
		first_scan = sscanf(token, "%d%c%d", start_index, &_slice_delimitor, end_index);

		if (first_scan <= 0) {
		// If the input was ":4" then no offsets will be set by the first sscanf,
		// so we check for this and try again with another format to match.
		// second scan will never be 0, since a digit can still be loaded into %c.
		second_scan = sscanf(token, "%c%d", &_slice_delimitor, end_index);
		}

		if (_slice_delimitor == ':') {
			*slice_detected = 1;

			// Handle if second slice arg is invalid
			if (first_scan == 2 || second_scan == 1) {
				// First_scan == 2:
				// Token could either be a parsable value such as '2:'
				// or an invalid value such as '2:abc'.
				// '2:' Should pass. first_slice_arg is 2. second_slice_arg is NULL
				// '2:abc' Should fail. first_slice_arg is 2. second_slice_arg is abc

				// Second_scan == 1
				// Token could either be a parsable value such as ':'
				// or an invalid value such as ':a'
				// ':' Should pass. first_slice_arg is NULL. second_slice_arg is NULL
				// ':a' Should fail. first_slice_arg is a. second_slice_arg is NULL

				char * saveptr2;
				char * first_slice_arg = strtok_r(token, ":", &saveptr2);
				char * second_slice_arg = strtok_r(NULL, ":", &saveptr2);

				if (first_slice_arg && !second_slice_arg) {
					// values such as: '2:' and ':a'.

					// Check if first slice arg is not a number
					// if it isnt a number, then value is invalid format such as: ':a'
					// If it is a number the value is valid format such as: '2:'
					char * endptr;
					if (strtoul(first_slice_arg, &endptr, 10) == 0 && strcmp(first_slice_arg, "0") != 0) {
						fprintf(stderr, "Second slice arg is invalid.\nCan only parse integers as indexes to slice by.\n");
						return -1;
					}

				} else if (first_slice_arg && second_slice_arg) {
					// Values such as: '2:abc'.
					// If both values are set and first_scan is 2 or second_scan is 1,
					// then an error has occured.
					fprintf(stderr, "Second slice arg is invalid.\nCan only parse integers as indexes to slice by.\n");
					return -1;
				}

			}

		} else if (second_scan > 0) {
			// If slice_delimitor is not ':' but second_scan still found something, then it must invalid input such as letters.
			fprintf(stderr, "Can only parse integers as indexes to slice by and ':' as delimitor.\n");
			return -1;
		} else if (first_scan >= 2) {
			// First scan found a number and a delimitor. But if the delimitor is not ':', then the input could be a decimal value, which is not allowed.
			fprintf(stderr, "Can only parse integers as indexes to slice by.\nSlice delimitor has to be ':'.\n");
			return -1;
		}

		if (second_scan > 0 && _slice_delimitor == ']') {
			// This is an error, example: set test_array[] 4
			fprintf(stderr, "Cannot set empty array slice.\n");
			return -1;
		}

	}

	// 5 outcomes:
	// 1. [4]    ->    first_scan == 2 | second_scan == 0
	// 2. [4:]   ->    first_scan == 2 | second_scan == 0
	// 3. [4:7]  ->    first_scan == 3 | second_scan == 0
	// 4. [:]    ->    first_scan == 0 | second_scan == 1
	// 5. [:7]   ->    first_scan == 0 | second_scan == 2
	// 1st and 2nd outcome we need to check on _slice_delimitor, if it is : then set slice_detected to 1
	return 0;
}

static int param_parse_from_str(int node, char * arg, param_t **param) {
	char *endptr;
	int id = strtoul(arg, &endptr, 10);
	// If strtoul has an error, then it will return ULONG_MAX, so we check on that.
	if (id != INT_MAX && *endptr == '\0') {
		*param = param_list_find_id(node, id);
	} else {
		*param = param_list_find_name(node, arg);
	}

	if (*param == NULL) {
		return -1;
	}

	return 0;
}

// Find out if amount of values is equal to any of the parsed amounts.
static int parse_slash_values(char ** args, int start_index, int expected_values_amount, ...) {
	va_list expected_values;
	va_start(expected_values, expected_values_amount);

	int amount_of_values = 0;

	for (int i = start_index; args[i] != NULL; i++) {
		amount_of_values++;

		if (amount_of_values > 99) {
			break;
		}

	}

	for (int i = 0; i < expected_values_amount; i++) {
		if (amount_of_values == va_arg(expected_values, int)) {
			va_end(expected_values);
			return 0;
		}

	}

	va_end(expected_values);
	va_start(expected_values, expected_values_amount);
	fprintf(stderr, "Value amount error. Got %d values. Expected:", amount_of_values);

	for (int i = 0; i < expected_values_amount - 1; i++) {
		int val = va_arg(expected_values, int);
		val = va_arg(expected_values, int);
		fprintf(stderr, " %d", val);
		if (i < expected_values_amount - 2) {
			fprintf(stderr, ",");
		}

	}

	fprintf(stderr, ".\n");
	va_end(expected_values);
	return -1;
}

static int param_get_offset_string(char * arg, char ** offsets) {
	char * saveptr;
	char * token;

	strtok_r(arg, "[", &saveptr);

	token = strtok_r(NULL, "[", &saveptr);
	if (token == NULL) {
		*offsets = NULL;
		return 0;
	}

	// Check if close bracket exists as last element in token.
	// If not, then return an error.
	if(token[strlen(token)-1] != ']'){
		fprintf(stderr, "Missing close bracket on array slice.\n");
		return -1;
	}

	// Check if slice input error, by checking if there are more characters after closing bracket.
	// Niche case, since closing bracket is detected as last element in the check above.
	// Case: set test_array_param[3]:5] | set test_array_param[3:]] 4
	int previous_token_length = strlen(token);
	strtok_r(token, "]", &saveptr);
	if ((previous_token_length - strlen(token)) > 1) {
		fprintf(stderr, "Slice input error. Cannot parse characters after closing bracket.\n");
		return -1;
	}
	*offsets = token;

	return 0;
}

static int param_offsets_parse_from_str(char * arg, int array_size, int *offset_array, int *expected_value_amount) {
	if (arg == NULL) {
		for (int i = 0; i < array_size; i++) {
			offset_array[i] = i;
		}
		*expected_value_amount = array_size;
		return 0;
	}

	int current_index = 0;
	char *token = strtok(arg,",");
	while (token != NULL) {
		if (current_index >= array_size) {
			fprintf(stderr, "Amount of value indexes entered is greater than param array size.\n");
			return -1;
		}

		char *semicolon_check = strchr(token, ':');
		if (semicolon_check != NULL) {
			int start_index = INT_MIN;
			int end_index = INT_MIN;
			int _slice_detected;

			if (param_slash_parse_slice(token, &start_index, &end_index, &_slice_detected) < 0) {
				return -1;
			}

			start_index = start_index != INT_MIN ? start_index : 0;
			end_index = end_index != INT_MIN ? end_index : array_size;
			if (start_index < 0) {
				start_index = array_size + start_index;
				// If the index is still less than 0, then we have an error.
				if (start_index < 0) {
					return -1;
				}

			}

			// Same goes for the end index.
			if (end_index < 0) {
				end_index = array_size + end_index;
				if (end_index < 0) {
					return -1;
				}

			}

			if (start_index >= end_index) {
				fprintf(stderr, "start index is greater than end index in array slice.\n");
				return SLASH_EINVAL;
			}

			if (end_index > array_size) {
				fprintf(stderr, "End index in slice is greater than param array size.\n");
				return SLASH_EINVAL;
			}

			for (int i = start_index; i < end_index; i++) {
				if (current_index >= array_size) {
					fprintf(stderr, "Amount of value indexes entered is greater than param array size.\n");
					return -1;
				}
				offset_array[current_index++] = i;
			}

		} else {
			int val = atoi(token);

			if (val < 0) {
				val = val + array_size;
			}

			offset_array[current_index++] = val;
		}

		token = strtok(NULL, ",");
	}
	*expected_value_amount = current_index;

	return 0;
}

static int parse_param_offset_string(char * arg_in,  int node, param_t **param, char ** arg_out) {
	if (param_get_offset_string(arg_in, arg_out) < 0) {
		return -1;
	}

	if (param_parse_from_str(node, arg_in, param) < 0) {
		if (*param == NULL) {
			fprintf(stderr, "%s not found.\n", arg_in);
		}

		return -1;
	}

	return 0;
}

static int hasDuplicates(int arr[], int size) {

	for (int i = 0; i < size; i++) {
		for (int j = i+1; j < size; j++) {
			if (arr[i] == arr[j]) {
				return 1;
			}
		}
	}

	return 0;
}


static void param_slash_parse(char * arg, int node, param_t **param, int *offset) {

	/* Search for the '@' symbol:
	* Call strtok twice in order to skip the stuff head of '@' */
	char * saveptr;
	char * token;

	/* Search for the '[' symbol: */
	strtok_r(arg, "[", &saveptr);
	token = strtok_r(NULL, "[", &saveptr);
	if (token != NULL) {
		sscanf(token, "%d", offset);
		*token = '\0';
	}

	char *endptr;
	int id = strtoul(arg, &endptr, 10);

	if (*endptr == '\0') {
		*param = param_list_find_id(node, id);
	} else {
		*param = param_list_find_name(node, arg);
	}

	return;
}

static void param_completer(struct slash *slash, char * token) {

	int matches = 0;
	size_t prefixlen = -1;
	param_t *prefix = NULL;
	char * orig_slash_buf = NULL;

	unsigned int node = slash_dfl_node;

	/* Build args */
	optparse_t * parser = optparse_new("", "");
	csh_add_node_option(parser, &node);
	int argi = optparse_parse(parser, slash->argc, (const char **) slash->argv);
	optparse_del(parser);

	/*
	This is really important: the original slash->buffer MUST be saved and restored to its original
	value, otherwise, there will be trouble whenever it is freed (usually during csh clean up)
	*/
	orig_slash_buf = slash->buffer;

	/* The following code basically skips the cmd line all the way up to the last non option argument,
	a.k.a. the very thing we're trying to complete */
	if(argi != -1) {
		/* If options where given, skip past them */
		if (argi == slash->argc) {
			/* All arguments were cmd line options or their arguments, point past them*/
			slash->buffer = slash->buffer + strlen(slash->buffer);
		} else if (argi == 0) {
			/* No cmd line options, "token" already points to what is to be completed */
			slash->buffer = token;
		} else {
			/* the remaining argument (at index slash->argc - 1) is leftovers after processing the options, a.k.a. the very thing we're trying to complete */
			slash->buffer = slash->buffer + strlen(slash->buffer) - strlen(slash->argv[slash->argc - 1]);
		}
	} else {
		/* Error parsing the options, bail out */
		return;
	}
	/* Skip possibly trailing whitespaces*/
	while(*slash->buffer == ' ') {
		slash->buffer++;
	}
	/* Finally, set "token" to the actual bit that needs completing */
	token = slash->buffer;

	size_t tokenlen = strlen(token);

	param_t * param;
	param_list_iterator i = { };
	bool found_completion = false;
	if (has_wildcard(token, strlen(token))) {
		// Only print parameters when globbing is involved.
		while ((param = param_list_iterate(&i)) != NULL) {
		if(node == *param->node) {
			if (strmatch(param->name, token, strlen(param->name), strlen(token))) {
				param_print(param, -1, NULL, 0, 2, 0);
				found_completion = true;
			}
		}
		}
		slash_completer_revert_skip(slash, orig_slash_buf);
		if(!found_completion) {
			printf("\nNo matching parameter found on node %d\n", node);
		}
		return;
	}

	while ((param = param_list_iterate(&i)) != NULL) {
	if(node != *param->node) {
		continue;
	}
		if (tokenlen > strlen(param->name))
			continue;

		if (strncmp(token, param->name, slash_min(strlen(param->name), tokenlen)) == 0
			&& *param->node == node) {

			/* Count matches */
			matches++;
			found_completion = true;

			/* Find common prefix */
			if (prefixlen == (size_t) -1) {
				prefix = param;
				prefixlen = strlen(prefix->name);
			} else {
				size_t new_prefixlen = slash_prefix_length(prefix->name,
						param->name);
				if (new_prefixlen < prefixlen)
					prefixlen = new_prefixlen;
			}

			/* Print newline on first match */
			if (matches == 1)
				slash_printf(slash, "\n");

			/* Print param */
			param_print(param, -1, NULL, 0, 2, 0);

		}

	}

	if (!matches) {
		printf("\nNo matching parameter found on node %d\n", node);
		slash_bell(slash);
	} else {
		strncpy(token, prefix->name, prefixlen);
		token[prefixlen] = 0;
		slash->cursor = slash->length = (token - slash->buffer) + prefixlen;
	}

	slash_completer_revert_skip(slash, orig_slash_buf);
}

static int cmd_get(struct slash *slash) {

	unsigned int node = slash_dfl_node;
	int paramver = 2;
	int server = 0;
	int prio = CSP_PRIO_NORM;

	optparse_t * parser = optparse_new("get", "<name>");
	optparse_add_help(parser);
	csh_add_node_option(parser, &node);
	optparse_add_custom(parser, 's', "server", "NUM", "server to get parameters from (default = node)", get_host_by_addr_or_name, &server);
	optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");
	optparse_add_int(parser, 'p', "prio", "NUM", 0, &prio, "CSP priority (0 = CRITICAL, 1 = HIGH, 2 = NORM (default), 3 = LOW)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Check if name is present */
	if (++argi >= slash->argc) {
		optparse_del(parser);
		printf("missing parameter name\n");
		return SLASH_EINVAL;
	}

	char * name = slash->argv[argi];
	int offset = -1;
	param_t * param = NULL;

	if (++argi != slash->argc) {
		optparse_del(parser);
		printf("too many arguments to command\n");
		return SLASH_EINVAL;
	}

	/* Go through the list of parameters */
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		/* Name match (with wildcard) */
		if (strmatch(param->name, name, strlen(param->name), strlen(name)) == 0) {
			continue;
		}

		/* Node match */
		if (*param->node != node) {
			continue;
		}

		/* Local parameters are printed directly */
		if ((*param->node == 0) && (server == 0)) {
			param_print(param, -1, NULL, 0, 0, 0);
			continue;
		}

		/* Select destination, host overrides parameter node */
		int dest = node;
		if (server > 0)
			dest = server;

		if (param_pull_single(param, offset, prio, 1, dest, slash_dfl_timeout, paramver) < 0) {
			printf("No response\n");
			optparse_del(parser);
			return SLASH_EIO;
		}

	}

	optparse_del(parser);
	return SLASH_SUCCESS;

}
static void param_get_cmd_completer(struct slash *slash, char * token) {
	param_completer(slash, token);
}

slash_command_completer(get, cmd_get, param_get_cmd_completer, "<param>", "Get");

static int cmd_set(struct slash *slash) {
	unsigned int node = slash_dfl_node;
	int paramver = 2;
	int server = 0;
	int ack_with_pull = true;
	int force = false;
	int prio = CSP_PRIO_NORM;

	optparse_t * parser = optparse_new("set", "<name>[offset] <value>");
	optparse_add_help(parser);
	csh_add_node_option(parser, &node);
	optparse_add_custom(parser, 's', "server", "NUM", "server to set parameters on (default = node)", get_host_by_addr_or_name, &server);
	optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");
	optparse_add_set(parser, 'a', "no_ack_push", 0, &ack_with_pull, "Disable ack with param push queue");
	optparse_add_set(parser, 'f', "force", true, &force, "force setting readonly params");
	optparse_add_int(parser, 'p', "prio", "NUM", 0, &prio, "CSP priority (0 = CRITICAL, 1 = HIGH, 2 = NORM (default), 3 = LOW)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Check if name is present */
	if (++argi >= slash->argc) {
		printf("missing parameter name\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * name = slash->argv[argi];

	param_t * param = NULL;

	// offset array, amount of possible offsets should be equal to param->array_size.
	// Default set to INT_MIN to determine if they've been set or not, since an offset can be < 0.
	int * offset_array = NULL;
	char * offset_token = NULL;

	// Get param name and offsets as a char array, if any offsets exist.
	if (parse_param_offset_string(name, node, &param, &offset_token) < 0) {
		fprintf(stderr, "Error when parsing offset string.\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (param == NULL) {
		fprintf(stderr, "%s not found\n", name);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	param->timestamp->tv_sec = 0; /* Reset parameter timestamp before adding to queue, to avoid serializing it. */
	int expected_value_amount = 0;

	// If param is of type data or string we only want to set on a single offset, that being -1.
	// Else we allocate memory for the offset array, fill it with INT_MIN, and parse the offset string to integers.
	// Lastly we set the expected_value_amount, this should match the amount of offsets put.
	if (param->type == PARAM_TYPE_DATA || param->type == PARAM_TYPE_STRING) {
		offset_array = (int *)malloc(sizeof(int));
		if(!offset_array) {
			fprintf(stderr,"Couldn't alloc 'offset_array', bailing out.\n");
			free(offset_array);
			optparse_del(parser);
			return SLASH_EINVAL;
		}
	  	offset_array[0] = -1;
		expected_value_amount = 1;

	 } else {
		offset_array = (int *)malloc(sizeof(int) * param->array_size);
		if(!offset_array) {
			fprintf(stderr,"Couldn't alloc 'offset_array', bailing out.\n");
			free(offset_array);
			optparse_del(parser);
			return SLASH_EINVAL;
		}
		for (int i = 0; i < param->array_size; i++) {
			offset_array[i] = INT_MIN;
		}

		if (param_offsets_parse_from_str(offset_token, param->array_size, offset_array, &expected_value_amount) < 0) {
			free(offset_array);
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		int *tmp = (int *)realloc(offset_array, sizeof(int)*expected_value_amount);
		if(tmp) {
		offset_array = tmp;
		} else {
		fprintf(stderr,"Couldn't realloc 'offset_array', bailing out.\n");
		free(offset_array);
		optparse_del(parser);
		return SLASH_EINVAL;
		}

		if(hasDuplicates(offset_array, expected_value_amount) == 1) {
			// offset_array has duplicate offsets. This should give an error.
			fprintf(stderr,"Offset array contain duplicate offsets.\n");
			free(offset_array);
			optparse_del(parser);
			return SLASH_EINVAL;
		}

	}

	offset_token = NULL;

	if (param->array_size == 1) {
		if (expected_value_amount > 1) {
			fprintf(stderr, "Cannot do array and slicing operations on a non-array parameter.\n");
			free(offset_array);
			optparse_del(parser);
			return SLASH_EINVAL;
		}
	}

	if (param->mask & PM_READONLY && !force) {
		printf("--force is required to set a readonly parameter\n");
		free(offset_array);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Check if Value is present */
	if (++argi >= slash->argc) {
		printf("missing parameter value\n");
		free(offset_array);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	// Parse expected amount of values
	// We expect an amount of values equal to the size of offset_array.
	// The size of offset_array can at most be equal to the param->array_size.
	if (parse_slash_values(slash->argv, argi, 2, 1, expected_value_amount) < 0) {
		free(offset_array);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	// Create a queue, so that we can set the param in a single packet.
	param_queue_t queue;
	char queue_buf[PARAM_SERVER_MTU];
	param_queue_init(&queue, queue_buf, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, 2);

	// We should iterate until we find an ending bracket ']'. Therefore we use the 'should_break' flag.
	int should_break = 1;
	int iterations = 0;
	int single_value_flag = 0;
	for (int i = argi; should_break == 1; i++) {
		char valuebuf[128] __attribute__((aligned(16))) = { };

		char *arg = slash->argv[i];
		if (!arg) {
			fprintf(stderr, "Missing closing bracket\n");
			free(offset_array);
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		// Check if we can find a start bracket '['.
		// If we can, then we're dealing with a value array.
		if (strchr(arg, '[')) {
			// If we're dealing with a value array, but only a single offset, then we should throw an error.
			if (expected_value_amount == 1) {
				fprintf(stderr, "cannot set array into indexed parameter value\n");
				free(offset_array);
				optparse_del(parser);
				return SLASH_EINVAL;
			}

			arg++;
			// A value array with a single element can also be passed.
			if (arg[strlen(arg)-1] == ']') {
				arg[strlen(arg)-1] = '\0';
				should_break = 0;
			}

		} else if (arg[strlen(arg)-1] == ']') {
			// Check if we can find an ending brakcet ']'.
			// If we can, then we should stop looping after the current iteration.
			arg[strlen(arg)-1] = '\0';
			should_break = 0;
		} else if (iterations == 0) {
			// If no bracket was found and the current iteration is the first iteration, then we are dealing with a single value.
			single_value_flag = 1;
		}

		// Convert the valuebuf str to a value of the param type.
		if (param_str_to_value(param->type, arg, valuebuf) < 0) {
			fprintf(stderr, "invalid parameter value\n");
			free(offset_array);
			optparse_del(parser);
			return SLASH_EINVAL;
		}


		// If we're dealing with a single value, we should loop the sliced array instead of the values.
		// Breaking afterwards, since this means we're done setting params.
		if (single_value_flag) {
			for (int j = 0; j < expected_value_amount; j++) {
				param_queue_add(&queue, param, offset_array[j], valuebuf);
			}

			break;
		}

		// If we're not dealing with a single value, we should look at iterations.
		// If iterations ever becomes greater than expected_value_amount, it means the amount of values are greater than the slice.
		if (iterations > expected_value_amount) {
			fprintf(stderr, "Values array is longer than value indexes\n");
			free(offset_array);
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		// Add to the queue.
		if (param_queue_add(&queue, param, offset_array[iterations], valuebuf) < 0) {
			fprintf(stderr, "Param_queue_add failed\n");
			free(offset_array);
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		iterations++;

		// If we hit the end value by detecting ']', and start index and end index are not equal,
		// then the slice length must be greater than the value array length.
		// This shouldnt be possible, so we throw an error.
		if (should_break == 0 && iterations != expected_value_amount) {
			fprintf(stderr, "Amount of value indexes is greater than value array\n");
			free(offset_array);
			optparse_del(parser);
			return SLASH_EINVAL;
		}

	}

	/* Local parameters are set directly */
	if (false && *param->node == 0) {
		param_queue_apply(&queue, 1, 0);

		// if (offset < 0 && param->type != PARAM_TYPE_STRING && param->type != PARAM_TYPE_DATA) {
		// 	for (int i = 0; i < param->array_size; i++)
		// 		param_set(param, i, valuebuf);
		// } else {
		// 	param_set(param, offset, valuebuf);
		// }

		param_print(param, -1, NULL, 0, 2, 0);
	} else {

		/* Select destination, host overrides parameter node */
		int dest = node;
		if (server > 0)
			dest = server;
		csp_timestamp_t time_now;
		csp_clock_get_time(&time_now);
		if (param_push_queue(&queue, CSP_PRIO_NORM, 0, dest, slash_dfl_timeout, 0, ack_with_pull) < 0) {
			printf("No response\n");
			optparse_del(parser);
			return SLASH_EIO;
		}
		param_print(param, -1, NULL, 0, 2, time_now.tv_sec);
	}

	free(offset_array);
	optparse_del(parser);
	return SLASH_SUCCESS;
}
static void param_set_cmd_completer(struct slash *slash, char * token) {
	param_completer(slash, token);
}
slash_command_completer(set, cmd_set, param_set_cmd_completer, "<param> <value>", "Set");


static int cmd_add(struct slash *slash) {

	unsigned int node = slash_dfl_node;
	char * include_mask_str = NULL;
	char * exclude_mask_str = NULL;
	int force = false;

	optparse_t * parser = optparse_new("cmd add", "<name>[offset] [value]");
	optparse_add_help(parser);
	csh_add_node_option(parser, &node);
	optparse_add_string(parser, 'm', "imask", "MASK", &include_mask_str, "Include mask (param letters) (used for get with wildcard)");
	optparse_add_string(parser, 'e', "emask", "MASK", &exclude_mask_str, "Exclude mask (param letters) (used for get with wildcard)");
	optparse_add_set(parser, 'f', "force", true, &force, "force setting readonly params");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_HWREG;

	if (include_mask_str)
		include_mask = param_maskstr_to_mask(include_mask_str);
	if (exclude_mask_str)
		exclude_mask = param_maskstr_to_mask(exclude_mask_str);

	/* Check if name is present */
	if (++argi >= slash->argc) {
		printf("missing parameter name\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (param_queue.type == PARAM_QUEUE_TYPE_SET) {

		char * name = slash->argv[argi];
		int offset = -1;
		param_t * param = NULL;
		param_slash_parse(name, node, &param, &offset);

		if (param == NULL) {
			printf("%s not found\n", name);
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		if (param->mask & PM_READONLY && !force) {
			printf("--force is required to set a readonly parameter\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		/* Check if Value is present */
		if (++argi >= slash->argc) {
			printf("missing parameter value\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		char valuebuf[128] __attribute__((aligned(16))) = { };
		if (param_str_to_value(param->type, slash->argv[argi], valuebuf) < 0) {
			printf("invalid parameter value\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}
		/* clear param timestamp so we dont set timestamp flag when serialized*/
		param->timestamp->tv_sec = 0;

		if (param_queue_add(&param_queue, param, offset, valuebuf) < 0)
			printf("Queue full\n");

	}

	if (param_queue.type == PARAM_QUEUE_TYPE_GET) {

		char * name = slash->argv[argi];
		int offset = -1;
		param_t * param = NULL;

		/* Go through the list of parameters */
		param_list_iterator i = {};
		while ((param = param_list_iterate(&i)) != NULL) {

			/* Name match (with wildcard) */
			if (strmatch(param->name, name, strlen(param->name), strlen(name)) == 0) {
				continue;
			}

			/* Node match */
			if (*param->node != node) {
				continue;
			}

			if (param->mask & exclude_mask) {
				continue;
			}

			if ((param->mask & include_mask) == 0) {
				continue;
			}

			/* Queue */
			if (param_queue_add(&param_queue, param, offset, NULL) < 0)
				printf("Queue full\n");
			continue;

		}

	}

	param_queue_print(&param_queue);
	optparse_del(parser);
	return SLASH_SUCCESS;
}
static void param_cmd_add_cmd_completer(struct slash *slash, char * token) {
	param_completer(slash, token);
}
slash_command_sub_completer(cmd, add, cmd_add, param_cmd_add_cmd_completer, "<param>[offset] [value]", "Add a new parameter to a command");


static int cmd_run(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;
	unsigned int hwid = 0;
	int ack_with_pull = true;
	int prio = CSP_PRIO_NORM;

	optparse_t * parser = optparse_new("run", "");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_custom(parser, 's', "server", "NUM", "server to push parameters to (default = <env>))", get_host_by_addr_or_name, &server);
	optparse_add_unsigned(parser, 'h', "hwid", "NUM", 16, &hwid, "include hardware id filter (default = off)");
	optparse_add_set(parser, 'a', "no_ack_push", 0, &ack_with_pull, "Disable ack with param push queue");
	optparse_add_int(parser, 'p', "prio", "NUM", 0, &prio, "CSP priority (0 = CRITICAL, 1 = HIGH, 2 = NORM (default), 3 = LOW)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (param_queue.type == PARAM_QUEUE_TYPE_SET) {

		csp_timestamp_t time_now;
		csp_clock_get_time(&time_now);
		if (param_push_queue(&param_queue, prio, 0, server, timeout, hwid, ack_with_pull) < 0) {
			printf("No response\n");
			optparse_del(parser);
			return SLASH_EIO;
		}
		param_queue_print_params(&param_queue, time_now.tv_sec);

	}

	if (param_queue.type == PARAM_QUEUE_TYPE_GET) {
		if (param_pull_queue(&param_queue, prio, 1, server, timeout)) {
			printf("No response\n");
			optparse_del(parser);
			return SLASH_EIO;
		}
	}


	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(cmd, run, cmd_run, "", NULL);

static int cmd_pull(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;
	char * include_mask_str = NULL;
	char * exclude_mask_str = NULL;
	char * nodes_str = NULL;
	int paramver = 2;
	int prio = CSP_PRIO_NORM;

	optparse_t * parser = optparse_new("pull", "");
	optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_unsigned(parser, 's', "server", "NUM", 0, &server, "server to pull parameters from (default = <env>))");
	optparse_add_string(parser, 'm', "imask", "MASK", &include_mask_str, "Include mask (param letters)");
	optparse_add_string(parser, 'e', "emask", "MASK", &exclude_mask_str, "Exclude mask (param letters)");
	optparse_add_string(parser, 'n', "nodes", "NODES", &nodes_str, "Comma separated list of nodes to pull parameters from");
	optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");
	optparse_add_int(parser, 'p', "prio", "NUM", 0, &prio, "CSP priority (0 = CRITICAL, 1 = HIGH, 2 = NORM (default), 3 = LOW)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_REMOTE | PM_HWREG;

	if (include_mask_str)
		include_mask = param_maskstr_to_mask(include_mask_str);
	if (exclude_mask_str)
		exclude_mask = param_maskstr_to_mask(exclude_mask_str);

	int result = SLASH_SUCCESS;
	uint8_t num_nodes = 0;
	uint16_t *nodes;
	if(NULL == nodes_str) {
		num_nodes++;
		nodes = calloc(num_nodes, sizeof(uint16_t));
		if(!nodes) {
			optparse_del(parser);
			return SLASH_ENOMEM;
		}
		nodes[0] = server;
	} else {
		char *cur_node = strdup(nodes_str);
		char *start = cur_node;
		char *end = cur_node + strlen(cur_node);
		uint8_t idx = 0;
		while(*cur_node) {
			if(*cur_node == ',') {
				*cur_node = 0;
				num_nodes++;
			}
			cur_node++;
		}
		num_nodes++;
		nodes = calloc(num_nodes, sizeof(uint16_t));
		uint16_t node_id = 0;
		cur_node = start;
		while(cur_node < end) {
			node_id = atoi(cur_node);
			if(node_id != 0) {
				nodes[idx++] = node_id;
			}
			while(*(++cur_node) != 0);
			cur_node++;
		}
		free(start);
		num_nodes = idx;
	}
	for (uint8_t i = 0; i < num_nodes; i++) {
		if (param_pull_all(prio, 1, nodes[i], include_mask, exclude_mask, timeout, paramver)) {
			printf("No response from %d\n", nodes[i]);
			result = SLASH_EIO;
		}
	}
	free(nodes);
	optparse_del(parser);
	return result;
}
slash_command(pull, cmd_pull, "[OPTIONS]", "Pull all metrics from given CSP node(s)");

static int cmd_new(struct slash *slash) {

	int paramver = 2;
	char *name = NULL;

	optparse_t * parser = optparse_new("cmd new", "<get/set> <cmd name>");
	optparse_add_help(parser);
	optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Must specify 'get' or 'set'\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Set/get */
	if (strncmp(slash->argv[argi], "get", 4) == 0) {
		param_queue.type = PARAM_QUEUE_TYPE_GET;
	} else if (strncmp(slash->argv[argi], "set", 4) == 0) {
		param_queue.type = PARAM_QUEUE_TYPE_SET;
	} else {
		printf("Must specify 'get' or 'set'\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Must specify a command name\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Command name */
	name = slash->argv[argi];
	strncpy(param_queue.name, name, sizeof(param_queue.name)-1);  // -1 to fit NULL byte

	csp_timestamp_t time_now;
	csp_clock_get_time(&time_now);
	param_queue.used = 0;
	param_queue.version = paramver;
	param_queue.last_timestamp.tv_sec = 0;
	param_queue.last_timestamp.tv_nsec = 0;
	param_queue.client_timestamp = time_now;

	printf("Initialized new command: %s\n", name);

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(cmd, new, cmd_new, "<get/set> <cmd name>", "Create a new command");


static int cmd_done(struct slash *slash) {
	param_queue.type = PARAM_QUEUE_TYPE_EMPTY;
	return SLASH_SUCCESS;
}
slash_command_sub(cmd, done, cmd_done, "", "Exit cmd edit mode");


static int cmd_print(struct slash *slash) {
	if (param_queue.type == PARAM_QUEUE_TYPE_EMPTY) {
		printf("No active command\n");
	} else {
		printf("Current command size: %d bytes\n", param_queue.used);
		param_queue_print(&param_queue);
	}
	return SLASH_SUCCESS;
}
slash_command(cmd, cmd_print, NULL, "Show current command");
