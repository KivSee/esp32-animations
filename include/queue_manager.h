#ifndef __QUEUE_MANAGER_H__
#define __QUEUE_MANAGER_H__

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class QueueManager {

public:
    QueueManager();

public:

    // queue from core 0 to 1, 
    // updating on a new runtime animation to execute in renderer
    QueueHandle_t runtime_animation_queue;

    // queue from core 0 to 1, updating on esp time synchronization event.
    // the payload is int64_t representing the epoch time of the esp start, e.g.
    // when esp millis() returned 0, this was the epoch time
    QueueHandle_t epoch_time_update_queue;

    // queue from core 0 to core 1
    // update on required global brightness to be used by renderer
    QueueHandle_t global_brightness_queue;

    // queue from core 1 to core 0
    // renderer letting know that it is done with this runtime animation
    // and it can be disposed of (release memory, invalidate cache etc)
    QueueHandle_t runtime_animation_delete_queue;

};

#endif //__QUEUE_MANAGER_H__