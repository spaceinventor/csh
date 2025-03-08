/*
 * param_scheduler.c
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#include <param/param_scheduler.h>

#include <stdio.h>
#include <csp/csp.h>
#include <csp/csp_hooks.h>
#include <csp/arch/csp_time.h>
#include <sys/types.h>
#include <string.h>

#include <param/param.h>
#include <param/param_queue.h>
#include <param/param_server.h>
#include <param/param_list.h>
#include <param/param_client.h>

#include <objstore/objstore.h>

#ifdef PARAM_HAVE_COMMANDS
#include <param/param_commands.h>

static param_command_buf_t temp_command;

static param_scheduler_meta_t meta_obj;

static param_schedule_buf_t temp_schedule;

/**
 * NOTE: The commands lock functions are external hooks,
 * and must therefore be implemented by the user.
 */
int si_lock_take(void* lock, int block_time_ms);
int si_lock_give(void* lock);
void* si_lock_init(void);

static void* lock = NULL;

static int find_meta_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);

    if (type == OBJ_TYPE_SCHEDULER_META)
        return -1;

    return 0;
}

/* Return CSP timestamp as uint64_t nanoseconds */
static uint64_t csp_clock_get_nsec(void) {
    csp_timestamp_t localtime;
	csp_clock_get_time(&localtime);
    return localtime.tv_sec * 1E9 + localtime.tv_nsec;
}

/* No internal mutex lock */
static void meta_obj_save(vmem_t * vmem) {
    int offset = objstore_scan(vmem, find_meta_scancb, 0, NULL);
    if (offset < 0)
        return;
    
    objstore_write_obj(vmem, offset, OBJ_TYPE_SCHEDULER_META, sizeof(meta_obj), (void *) &meta_obj);
}

static int next_schedule_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_SCHEDULE)
        return 0;
    
    int length = objstore_read_obj_length(vmem, offset);
    if (length < 0)
        return 0;

    /* Break iteration after the correct number of objects */
    if (*(int*)ctx == 0) {
        return -1;
    } else {
        *(int*)ctx -= 1;
    }

    return 0;
}

static int num_schedule_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_SCHEDULE)
        return 0;

    /* Found a schedule object, increment ctx */
    *(int*)ctx += 1;

    return 0;
}

static int get_number_of_schedule_objs(vmem_t * vmem) {
    int num_schedule_objs = 0;

    objstore_scan(vmem, num_schedule_scancb, 0, (void *) &num_schedule_objs);

    return num_schedule_objs;
}

/* Remove all completed schedules */
static void schedule_rm_complete(void) {
    int num_schedules = get_number_of_schedule_objs(&vmem_schedule);
    
    /* Iterate over the number of schedules and erase each completed one */
    int uncomplete_scheds_found = 0;
    for (int i = 0; i < num_schedules; i++) {
        int obj_skips = uncomplete_scheds_found;
        int offset = objstore_scan(&vmem_schedule, next_schedule_scancb, 0, (void *) &obj_skips);
        if (offset < 0) {
            continue;
        }
        uint8_t completed;
        vmem_schedule.read(&vmem_schedule, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t,completed), &completed, sizeof(completed));

        if (completed != 0) {
            objstore_rm_obj(&vmem_schedule, offset, 0);
        } else {
            uncomplete_scheds_found++;
        }
    }
}

static uint16_t schedule_add(csp_packet_t *packet, param_queue_type_e q_type) {
    /* Return if packet length is too short to be valid */
    if (packet->length < 12){
        return UINT16_MAX;
    }
    
    int queue_size = packet->length - 12;

    /* Determine schedule size and allocate VMEM */
    int obj_length = (int) sizeof(param_schedule_t) + queue_size - (int) sizeof(char *);

    /* Return in case of failure to retrieve lock */
    if (si_lock_take(lock, 1000) != 0) {
        printf("Lock timeout in %s\n", __FUNCTION__);
        return UINT16_MAX;
    }
    
    int obj_offset = objstore_alloc(&vmem_schedule, obj_length, 0);
    if (obj_offset < 0) {
        /* Remove all completed schedules */
        schedule_rm_complete();

        /* Try allocating again */
        if ( (obj_offset = objstore_alloc(&vmem_schedule, obj_length, 0)) < 0) {
            si_lock_give(lock);
            return UINT16_MAX;
        }
    }
    
    memset(&temp_schedule, 0, sizeof(temp_schedule));

    temp_schedule.header.active = 0x55;
    temp_schedule.header.completed = 0;
    // Each schedule can retry up to 5 times in case of communications issues
    temp_schedule.header.retries = 5; 

    int meta_offset = objstore_scan(&vmem_schedule, find_meta_scancb, 0, NULL);
    if (meta_offset < 0) {
        si_lock_give(lock);
        return UINT16_MAX;
    }
    
    if (objstore_read_obj(&vmem_schedule, meta_offset, (void *) &meta_obj, 0) < 0){
        printf("Meta obj checksum error\n");
    }
    meta_obj.last_id++;
    if (meta_obj.last_id == UINT16_MAX){
        meta_obj.last_id = 0;
    }
    meta_obj_save(&vmem_schedule);
    temp_schedule.header.id = meta_obj.last_id;

	uint64_t timestamp = csp_clock_get_nsec();

    temp_schedule.header.time = (uint64_t) be32toh(packet->data32[1])*1E9;
    if (temp_schedule.header.time <= 1E18) {
        temp_schedule.header.time += timestamp;
    }
    temp_schedule.header.latency_buffer = be32toh(packet->data32[2]);

    temp_schedule.header.host = be16toh(packet->data16[1]);

    /* Initialize schedule queue and copy queue buffer from CSP packet */
    param_queue_init(&temp_schedule.header.queue, (char *) &temp_schedule + sizeof(param_schedule_t), queue_size, queue_size, q_type, 2);
    memcpy(temp_schedule.header.queue.buffer, &packet->data[12], temp_schedule.header.queue.used);

    void * write_ptr = (void *) (long int) &temp_schedule + sizeof(temp_schedule.header.queue.buffer);
    objstore_write_obj(&vmem_schedule, obj_offset, (uint8_t) OBJ_TYPE_SCHEDULE, (uint8_t) obj_length, write_ptr);

    si_lock_give(lock);
    
    return meta_obj.last_id;
}

int param_serve_schedule_push(csp_packet_t *request) {
    csp_packet_t * response = csp_buffer_get(4);
    if (response == NULL) {
        csp_buffer_free(request);
        return -1;
    }

    uint16_t id = schedule_add(request, PARAM_QUEUE_TYPE_SET);

    /* Send ack with ID */
	response->data[0] = PARAM_SCHEDULE_ADD_RESPONSE;
	response->data[1] = PARAM_FLAG_END;
    response->data16[1] = htobe16(id);
	response->length = 4;

	csp_sendto_reply(request, response, CSP_O_SAME);

    csp_buffer_free(request);

    return 0;
}

#if 0
// not updated with objstore
int param_serve_schedule_pull(csp_packet_t *request) {
    csp_packet_t * response = csp_buffer_get(0);
    if (response == NULL) {
        csp_buffer_free(request);
        return -1;
    }

    csp_packet_t * request_copy = csp_buffer_clone(request);
    uint16_t id = schedule_add(request_copy, PARAM_QUEUE_TYPE_GET);

    /* Send ack with ID */
	response->data[0] = PARAM_SCHEDULE_ADD_RESPONSE;
	response->data[1] = PARAM_FLAG_END;
    response->data16[1] = htobe16(id);
	response->length = 4;

	csp_sendto_reply(request, response, CSP_O_SAME);

    csp_buffer_free(request);

    return 0;
}
#endif

static int obj_offset_from_id_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int target_id =  *(int*)ctx;

    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_SCHEDULE)
        return 0;

    uint16_t id;
    vmem->read(vmem, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t,id), &id, sizeof(id));
    
    if (id == target_id) {
        return -1;
    }

    return 0;
}

static int obj_offset_from_id(vmem_t * vmem, int id) {
    int offset = objstore_scan(vmem, obj_offset_from_id_scancb, 0, (void*) &id);
    return offset;
}

int param_serve_schedule_show(csp_packet_t *packet) {
    uint16_t id = be16toh(packet->data16[1]);
    int status = 0;

    if (si_lock_take(lock, 1000) != 0) {
        printf("Lock timeout in %s\n", __FUNCTION__);
        status = -1;  /* Failed to retrieve lock */
    }

    int offset, length;
    if (status == 0) {
        offset = obj_offset_from_id(&vmem_schedule, id);
        if (offset < 0)
            status = -1;
    }

    if (status == 0) {
        length = objstore_read_obj_length(&vmem_schedule, offset);
        if (length < 0)
            status = -1;
    }
    
    if (status == 0) {
        /* Read the schedule entry */
        //param_schedule_t * temp_schedule = malloc(length + sizeof(temp_schedule->queue.buffer));
        memset(&temp_schedule, 0, sizeof(temp_schedule));
        void * read_ptr = (void*) ( (long int) &temp_schedule + sizeof(temp_schedule.header.queue.buffer));
        objstore_read_obj(&vmem_schedule, offset, read_ptr, 0);

        temp_schedule.header.queue.buffer = (char *) ((long int) &temp_schedule + (long int) (sizeof(param_schedule_t)));

        /* Respond with the requested schedule entry */
        packet->data[0] = PARAM_SCHEDULE_SHOW_RESPONSE;
        packet->data[1] = PARAM_FLAG_END;

        packet->data16[1] = htobe16(temp_schedule.header.id);
        uint32_t time_s = (uint32_t) (temp_schedule.header.time / 1E9);
        packet->data32[1] = htobe32(time_s);
        packet->data[8] = temp_schedule.header.queue.type;
        packet->data[9] = temp_schedule.header.completed;

        packet->length = temp_schedule.header.queue.used + 10;
        
        memcpy(&packet->data[10], temp_schedule.header.queue.buffer, temp_schedule.header.queue.used);
        
        si_lock_give(lock);

    } else {
        si_lock_give(lock);

        /* Respond with an error code */
        packet->data[0] = PARAM_SCHEDULE_SHOW_RESPONSE;
        packet->data[1] = PARAM_FLAG_END;

        packet->data16[1] = htobe16(UINT16_MAX);

        packet->length = 4;
    }

	csp_sendto_reply(packet, packet, CSP_O_SAME);

    return 0;
}

int param_serve_schedule_rm_single(csp_packet_t *packet) {
    /* Disable the specified schedule id */
    uint16_t id = be16toh(packet->data16[1]);
    if (si_lock_take(lock, 1000) != 0) {
        printf("Lock timeout in %s\n", __FUNCTION__);
        return -1;  /* Failed to retrieve lock */
    }
    
    int offset = obj_offset_from_id(&vmem_schedule, id);
    if (offset < 0) {
        si_lock_give(lock);
        csp_buffer_free(packet);
        return -1;
    }

    if (objstore_rm_obj(&vmem_schedule, offset, 0) < 0) {
        si_lock_give(lock);
        csp_buffer_free(packet);
        return -1;
    }

    si_lock_give(lock);
    
    /* Respond with the id again to verify which ID was erased */
	packet->data[0] = PARAM_SCHEDULE_RM_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = htobe16(id);
    
	packet->length = 4;

	csp_sendto_reply(packet, packet, CSP_O_SAME);

    return 0;
}

int param_serve_schedule_rm_all(csp_packet_t *packet) {
    /* Confirm remove all by checking that id = UINT16_MAX */
    uint16_t id = be16toh(packet->data16[1]);
    if (id != UINT16_MAX) {
        csp_buffer_free(packet);
        return -1;
    }

    if (si_lock_take(lock, 1000) != 0) {
        printf("Lock timeout in %s\n", __FUNCTION__);
        return -1;  /* Failed to retrieve lock */
    }
    
    int num_schedules = get_number_of_schedule_objs(&vmem_schedule);
    uint16_t deleted_schedules = 0;
    /* Iterate over the number of schedules and erase each */
    for (int i = 0; i < num_schedules; i++) {
        int obj_skips = 0;
        int offset = objstore_scan(&vmem_schedule, next_schedule_scancb, 0, (void *) &obj_skips);
        if (offset < 0) {
            continue;
        }

        if (objstore_rm_obj(&vmem_schedule, offset, 0) < 0) {
            continue;
        }
        deleted_schedules++;
    }

    si_lock_give(lock);

    /** Respond with id = UINT16_MAX again to verify that the schedule was cleared
     * include the number of schedules deleted in the response
     */
	packet->data[0] = PARAM_SCHEDULE_RM_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = htobe16(UINT16_MAX);
    packet->data16[2] = htobe16(deleted_schedules);

	packet->length = 6;

	csp_sendto_reply(packet, packet, CSP_O_SAME);

    return 0;
}

int param_serve_schedule_list(csp_packet_t *request) {
    
    if (si_lock_take(lock, 1000) != 0) {
        printf("Lock timeout in %s\n", __FUNCTION__);
        return -1;  /* Failed to retrieve lock */
    }

    int num_schedules = get_number_of_schedule_objs(&vmem_schedule);
    unsigned int counter = 0;
    unsigned int big_count = 0;
    int end = 0;

    while (end == 0) {
        csp_packet_t * response = csp_buffer_get(PARAM_SERVER_MTU);
        if (response == NULL)
            break;

        int num_per_packet = (PARAM_SERVER_MTU-4)/8;
        
        if ( (num_schedules - big_count*num_per_packet)*8 < (PARAM_SERVER_MTU - 4))
            end = 1;

        int loop_max = (big_count+1)*num_per_packet;
        if (end == 1)
            loop_max = num_schedules;

        for (int i = big_count*num_per_packet; i < loop_max; i++) {
            int obj_skips = counter;
            int offset = objstore_scan(&vmem_schedule, next_schedule_scancb, 0, (void *) &obj_skips);
            if (offset < 0) {
                counter++;
                continue;
            }

            int length = objstore_read_obj_length(&vmem_schedule, offset);
            if (length < 0) {
                counter++;
                continue;
            }
            //param_schedule_t * temp_schedule = malloc(length + sizeof(temp_schedule->queue.buffer));
            memset(&temp_schedule, 0, sizeof(temp_schedule));
            void * read_ptr = (void*) ( (long int) &temp_schedule + sizeof(temp_schedule.header.queue.buffer));
            objstore_read_obj(&vmem_schedule, offset, read_ptr, 0);

            unsigned int idx = 4+(counter-big_count*num_per_packet)*(4+2+2);

            uint32_t time_s = (uint32_t) (temp_schedule.header.time / 1E9);
            response->data32[idx/4] = htobe32(time_s);
            response->data16[idx/2+2] = htobe16(temp_schedule.header.id);
            response->data[idx+6] = temp_schedule.header.completed;
            response->data[idx+7] = temp_schedule.header.queue.type;
            
            counter++;
        }

        response->data[0] = PARAM_SCHEDULE_LIST_RESPONSE;
        if (end == 1) {
            si_lock_give(lock);
            response->data[1] = PARAM_FLAG_END;
        } else {
            response->data[1] = 0;
        }
        response->data16[1] = htobe16(counter-big_count*num_per_packet); // number of entries
        response->length = (counter-big_count*num_per_packet)*8 + 4;

        csp_sendto_reply(request, response, CSP_O_SAME);

        big_count++;
    }

    si_lock_give(lock);
    csp_buffer_free(request);

    return 0;
}

void param_serve_schedule_reset(csp_packet_t *packet) {
    if (be16toh(packet->data16[1]) != 0) {
        meta_obj.last_id = be16toh(packet->data16[1]);
    }

    if (si_lock_take(lock, 1000) != 0) {
        printf("Lock timeout in %s\n", __FUNCTION__);
        return;  /* Failed to retrieve lock */
    }

    meta_obj_save(&vmem_schedule);

    si_lock_give(lock);

    packet->data[0] = PARAM_SCHEDULE_RESET_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;

    packet->data16[1] = htobe16(meta_obj.last_id);
	
    packet->length = 4;

	csp_sendto_reply(packet, packet, CSP_O_SAME);
}

#ifdef PARAM_HAVE_COMMANDS
static void name_copy(char output[], char input[], int length) {
    for (int i = 0; i < length; i++) {
			output[i] = input[i];
    }
    output[length] = '\0';
}

static uint16_t schedule_command(csp_packet_t *packet) {
    if (packet->length < 13)
        return UINT16_MAX;
    
    /* Find and read the requested command */
    int name_length = packet->length - 12;
    char name[14] = {0};
    name_copy(name, (char *) &packet->data[12], name_length);

    if (si_lock_take(lock, 1000) != 0) {
        printf("Lock timeout in %s\n", __FUNCTION__);
        return UINT16_MAX;  /* Failed to retrieve lock */
    }

    memset(&temp_command, 0, sizeof(temp_command));
    if(param_command_read(name, &temp_command) < 0) {
        si_lock_give(lock);
        return UINT16_MAX;
    }

    int queue_size = temp_command.header.queue.used;

    /* Determine schedule size and allocate VMEM */
    int obj_length = (int) sizeof(param_schedule_t) + queue_size - (int) sizeof(char *);

    int obj_offset = objstore_alloc(&vmem_schedule, obj_length, 0);
    if (obj_offset < 0) {
        si_lock_give(lock);
        return UINT16_MAX;
    }

    //param_schedule_t * temp_schedule = malloc(sizeof(param_schedule_t) + queue_size);
    memset(&temp_schedule, 0, sizeof(temp_schedule));

    temp_schedule.header.active = 0x55;
    temp_schedule.header.completed = 0;

    int meta_offset = objstore_scan(&vmem_schedule, find_meta_scancb, 0, NULL);
    if (meta_offset < 0) {
        si_lock_give(lock);
        return UINT16_MAX;
    }
    
    if (objstore_read_obj(&vmem_schedule, meta_offset, (void *) &meta_obj, 0) < 0)
        printf("Meta obj checksum error\n");
    meta_obj.last_id++;
    if (meta_obj.last_id == UINT16_MAX){
        meta_obj.last_id = 0;
    }
    meta_obj_save(&vmem_schedule);
    temp_schedule.header.id = meta_obj.last_id;

	uint64_t timestamp = csp_clock_get_nsec();

    temp_schedule.header.time = (uint64_t) be32toh(packet->data32[1])*1E9;
    if (temp_schedule.header.time <= 1E18) {
        temp_schedule.header.time += timestamp;
    }
    temp_schedule.header.latency_buffer = be32toh(packet->data32[2]);

    temp_schedule.header.host = be16toh(packet->data16[1]);

    /* Initialize schedule queue and copy queue buffer from CSP packet */
    param_queue_init(&temp_schedule.header.queue, (char *) &temp_schedule + sizeof(param_schedule_t), queue_size, queue_size, temp_command.header.queue.type, 2);
    memcpy(temp_schedule.header.queue.buffer, temp_command.header.queue.buffer, temp_schedule.header.queue.used);

    void * write_ptr = (void *) (long int) &temp_schedule + sizeof(temp_schedule.header.queue.buffer);
    objstore_write_obj(&vmem_schedule, obj_offset, (uint8_t) OBJ_TYPE_SCHEDULE, (uint8_t) obj_length, write_ptr);
    
    si_lock_give(lock);

    return meta_obj.last_id;
}

int param_serve_schedule_command(csp_packet_t *request) {
    csp_packet_t * response = csp_buffer_get(4);
    if (response == NULL) {
        csp_buffer_free(request);
        return -1;
    }

    uint16_t id = schedule_command(request);

    /* Send ack with ID */
	response->data[0] = PARAM_SCHEDULE_ADD_RESPONSE;
	response->data[1] = PARAM_FLAG_END;
    response->data16[1] = htobe16(id);
	response->length = 4;

	csp_sendto_reply(request, response, CSP_O_SAME);

    csp_buffer_free(request);

    return 0;
}
#endif

static int find_inactive_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_SCHEDULE)
        return 0;
    
    int length = objstore_read_obj_length(vmem, offset);
    if (length < 0)
        return 0;

    uint8_t active;
    vmem->read(vmem, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, active), &active, sizeof(active));

    if (active == 0) {
        return -1;
    }

    return 0;
}

int param_schedule_server_update(uint64_t timestamp_nsec) {

    if (si_lock_take(lock, 1000) != 0) {
        printf("Lock timeout in %s\n", __FUNCTION__);
        return -1;  /* Failed to retrieve lock */
    }

    int num_schedules = get_number_of_schedule_objs(&vmem_schedule);
    /* Check the time on each schedule and execute if deadline is exceeded */ 
    for (int i = 0; i < num_schedules; i++) {
        int obj_skips = i;
        int offset = objstore_scan(&vmem_schedule, next_schedule_scancb, 0, (void *) &obj_skips);
        if (offset < 0) {
            continue;
        }

        uint8_t completed;
        vmem_schedule.read(&vmem_schedule, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t,completed), &completed, sizeof(completed));
        uint64_t time;
        vmem_schedule.read(&vmem_schedule, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t,time), &time, sizeof(time));

        if (completed == 0) {
            if (time <= timestamp_nsec) {
                uint32_t latency_buffer;
                vmem_schedule.read(&vmem_schedule, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t,latency_buffer), &latency_buffer, sizeof(latency_buffer));

                if ( (latency_buffer*1E9 >= (timestamp_nsec - time))  || (latency_buffer == 0) ) {
                    /* Read the whole schedule object */
                    int length = objstore_read_obj_length(&vmem_schedule, offset);
                    if (length < 0) {
                        continue;
                    }
                    memset(&temp_schedule, 0, sizeof(temp_schedule));
                    void * read_ptr = (void*) ( (long int) &temp_schedule + sizeof(temp_schedule.header.queue.buffer));
                    objstore_read_obj(&vmem_schedule, offset, read_ptr, 0);

                    temp_schedule.header.queue.buffer = (char *) ((long int) &temp_schedule + (long int) (sizeof(param_schedule_t)));

                    /* Execute the scheduled queue */
                    if (param_push_queue(&temp_schedule.header.queue, CSP_PRIO_NORM, 0, temp_schedule.header.host, 1000, 0, false) == 0){
                        temp_schedule.header.completed = 0x55;
                        temp_schedule.header.time = timestamp_nsec;
                    } else {
                        if (temp_schedule.header.retries > 0) {
                            /* Postpone 5 seconds to retry in case of network errors */
                            temp_schedule.header.time += 5*1E9;
                            temp_schedule.header.retries--;
                        } else {
                            /* Out of retries */
                            completed = 0xAA;
                            objstore_write_data(&vmem_schedule, offset, OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, completed), sizeof(completed), &completed);
                            objstore_write_data(&vmem_schedule, offset, OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, time), sizeof(time), &timestamp_nsec);
                        }
                    }

                    /* Write the results to objstore */
                    void * write_ptr = (void*) ( (long int) &temp_schedule + sizeof(temp_schedule.header.queue.buffer));
                    objstore_write_obj(&vmem_schedule, offset, OBJ_TYPE_SCHEDULE, length, write_ptr);
                } else {
                    /* Latency buffer exceeded */
                    completed = 0xAA;
                    objstore_write_data(&vmem_schedule, offset, OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, completed), sizeof(completed), &completed);
                    objstore_write_data(&vmem_schedule, offset, OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, time), sizeof(time), &timestamp_nsec);
                }
            }
        } else {
            if (time <= (timestamp_nsec - (uint64_t) 86400*1E9)) {
                /* Deactivate completed schedule entries after 24 hrs */
                uint8_t active = 0;
                objstore_write_data(&vmem_schedule, offset, OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, active), sizeof(active), &active);
            }
        }
    }

    si_lock_give(lock);

    /* Deleting inactive schedules is low priority, only run if no other tasks are waiting to use the vmem */
    if (si_lock_take(lock, 0) != 0) {
        printf("Lock timeout in %s\n", __FUNCTION__);
        return -1;  /* Failed to retrieve lock */
    }

    /* Delete inactive schedules */
    for (int i = 0; i < num_schedules; i++) {
        int offset = objstore_scan(&vmem_schedule, find_inactive_scancb, 0, NULL);
        if (offset < 0)
            break;

        objstore_rm_obj(&vmem_schedule, offset, 0);
    }

    si_lock_give(lock);

    //printf("Schedule server update completed.\n");
    return 0;
}

static void meta_obj_init(vmem_t * vmem) {
    /* Search for scheduler meta object */
    if (si_lock_take(lock, -1) != 0) {  // Use longest possible timeout
        printf("Lock timeout in %s\n", __FUNCTION__);
        return;
    }    

    int offset = objstore_scan(vmem, find_meta_scancb, 0, NULL);
    
    if (offset < 0) {
        /* Not found, initialize default values and write the meta object */
        meta_obj.last_id = UINT16_MAX;
        offset = objstore_alloc(vmem, sizeof(meta_obj), 0);
        objstore_write_obj(vmem, offset, OBJ_TYPE_SCHEDULER_META, sizeof(meta_obj), (void *) &meta_obj);
    } else {
        if (objstore_read_obj(vmem, offset, (void *) &meta_obj, 0) < 0) {
            /* Invalid meta object checksum, reset to highest ID among schedules */
            uint16_t max_id = 0;
            int num_schedules = get_number_of_schedule_objs(vmem);
            for (int i = 0; i < num_schedules; i++) {
                int obj_skips = i;
                int offset = objstore_scan(vmem, next_schedule_scancb, 0, (void *) &obj_skips);
                if (offset < 0) {
                    continue;
                }

                uint16_t read_id;
                vmem->read(vmem, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, id), &read_id, sizeof(read_id));
                if (read_id > max_id)
                    max_id = read_id;
            }
            
            if (num_schedules <= 0) {
                meta_obj.last_id = UINT16_MAX;
            } else {
                meta_obj.last_id = max_id;
            }
            
            objstore_write_obj(vmem, offset, OBJ_TYPE_SCHEDULER_META, sizeof(meta_obj), (void *) &meta_obj);
        }
    }

    si_lock_give(lock);
}

void param_schedule_server_init(void) {
    lock = si_lock_init();
    meta_obj_init(&vmem_schedule);
}
#endif
