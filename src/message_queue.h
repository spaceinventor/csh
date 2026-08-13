#pragma once

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS_QUEUE_SIZE 84 /* Must match the size of a StaticQueue_t object from FreeRTOS */

typedef struct message_queue_s {
    uint8_t obj[OS_QUEUE_SIZE];
    void *handle;
} message_queue_t;

extern void message_queue_create(message_queue_t *me, uint32_t item_size, uint32_t length, void *storage);
extern int message_queue_send(message_queue_t *me, void *item);
extern int message_queue_send_isr(message_queue_t *me, void *item);
extern int message_queue_receive(message_queue_t *me, void *item);
extern int message_queue_receive_wto(message_queue_t *me, void *item, uint32_t to_ms);

#ifdef __cplusplus
}
#endif
