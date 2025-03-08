/*
 * param_commands_client.h
 *
 *  Created on: Sep 22, 2021
 *      Author: Mads
 */

#pragma once

#include <param/param.h>
#include <param/param_queue.h>

int param_command_push(param_queue_t *queue, int verbose, int server, char command_name[], int timeout);
int param_command_download(int server, int verbose, char command_name[], int timeout);
int param_command_list(int server, int verbose, int timeout);
int param_command_rm(int server, int verbose, char command_name[], int timeout);
int param_command_rm_all(int server, int verbose, char command_name[], int timeout);