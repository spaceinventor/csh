/*
 * param_list_store_slash.c
 *
 *  Created on: Jan 4, 2019
 *      Author: johan
 */

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <slash/slash.h>

#include <param/param.h>
#include <param/param_list.h>
#include "param_list.h"
#include "param_list_store.h"


static int param_list_store_vmem_save_slash(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;

	int id = atoi(slash->argv[1]);
	param_list_store_vmem_save(vmem_index_to_ptr(id));
	return SLASH_SUCCESS;
}
slash_command_sub(list, save, param_list_store_vmem_save_slash, "<vmem_id>", NULL);

static int param_list_store_vmem_load_slash(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;

	int id = atoi(slash->argv[1]);
	param_list_store_vmem_load(vmem_index_to_ptr(id));

	return SLASH_SUCCESS;
}
slash_command_sub(list, load, param_list_store_vmem_load_slash, "<vmem_id>", NULL);

