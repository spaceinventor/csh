#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "message_queue.h"


#define OSSI_QUEUE_NAME_TEMPLATE "/tmp/ossi_%p"
#define QUEUE_PERMISSIONS 0660

static pthread_mutex_t g_crit_init_mtx = PTHREAD_MUTEX_INITIALIZER;


static char *queue_files_to_clean[100];
static uint8_t queue_count = 0;

static void message_queue_cleanup(void) {    
    for (uint8_t i = 0; i < queue_count; i++) {
        unlink(queue_files_to_clean[i]);
        free(queue_files_to_clean[i]);
    }
}

typedef struct systemv_ossi_queue_s {
    uint32_t item_size;
} systemv_ossi_queue_t;

void message_queue_create(message_queue_t *me, uint32_t item_size, uint32_t length, void *storage) {
    (void)length;
    (void)storage;
    
    key_t msg_queue_key;
    systemv_ossi_queue_t *sysv_queue = (systemv_ossi_queue_t *)me->obj;

    sysv_queue->item_size = item_size;
    char *queue_path_based_on_me = calloc(1, PATH_MAX);
    if(!queue_path_based_on_me)  {
        perror("message_queue_create: calloc() failure");
        exit(0);
    } else {
        snprintf(queue_path_based_on_me, PATH_MAX - 1, OSSI_QUEUE_NAME_TEMPLATE, (void *)me);
    }
    int q_fd = open(queue_path_based_on_me, O_CREAT | O_EXCL, 0660);
    if(q_fd) {
        close(q_fd);
    }
    int qid;

    if ((msg_queue_key = ftok (queue_path_based_on_me, getpid())) == -1) {
        free(queue_path_based_on_me);
        perror ("message_queue_create: ftok() failed");
        exit (1);
    }

    if ((qid = msgget (msg_queue_key, IPC_CREAT | QUEUE_PERMISSIONS)) == -1) {
        free(queue_path_based_on_me);
        perror ("message_queue_create: msgget() failed");
        exit (1);
    }
    pthread_mutex_lock(&g_crit_init_mtx);
    if(queue_count == 0) {
        atexit(message_queue_cleanup);
    }
    if(queue_count < 100) {
        queue_files_to_clean[queue_count++] = queue_path_based_on_me;    
    } else {
        free(queue_path_based_on_me);
        perror ("message_queue_create: too many queues (100 max)");
        pthread_mutex_unlock(&g_crit_init_mtx);
        exit (1);
    }
    pthread_mutex_unlock(&g_crit_init_mtx);
    /* Bit nasty, this. Using the handle to store the queue id */
    memcpy(&me->handle, &qid, sizeof(qid));
}

int message_queue_send(message_queue_t *me, void *item) {   
    systemv_ossi_queue_t *sysv_queue = (systemv_ossi_queue_t *)me->obj; 
    if (msgsnd ((int)(intptr_t)me->handle, item, sysv_queue->item_size, 0) == -1) {
        perror ("message_queue_send: msgsnd() failed");
        return 1;
    }
    return 0;
}

int message_queue_send_isr(message_queue_t *me, void *item) {
    return message_queue_send(me, item);
}

int message_queue_receive(message_queue_t *me, void *item) {
    systemv_ossi_queue_t *sysv_queue = (systemv_ossi_queue_t *)me->obj; 
    if (msgrcv ((int)(intptr_t)me->handle, item, sysv_queue->item_size, 0, 0) == -1) {
        perror ("message_queue_receive: msgrcv() failed");
        return 1;
    }
    return 0;
}
