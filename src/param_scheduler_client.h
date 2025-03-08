/*
 * param_scheduler_client.c
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#pragma once

#include <param/param.h>
#include <param/param_queue.h>

int param_schedule_push(param_queue_t *queue, int verbose, int server, uint16_t host, uint32_t time, uint32_t latency_buffer, int timeout);
int param_schedule_pull(param_queue_t *queue, int verbose, int server, uint16_t host, uint32_t time, int timeout);
int param_show_schedule(int server, int verbose, uint16_t id, int timeout);
int param_list_schedule(int server, int verbose, int timeout);
int param_rm_schedule(int server, int verbose, uint16_t id, int timeout);
int param_rm_all_schedule(int server, int verbose, int timeout);
int param_reset_scheduler(int server, uint16_t last_id, int verbose, int timeout);
int param_schedule_command(int verbose, int server, char command_name[], uint16_t host, uint32_t time, uint32_t latency_buffer, int timeout);