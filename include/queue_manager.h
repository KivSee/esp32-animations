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

    // queue from core 1 to core 0
    // periodically send metrics about rendering to core 0 for reporting
    QueueHandle_t core1_metrics_queue;

    #if defined(KIVSEE_DEBUG)
    // queue from core 0 to core 1
    // send the latest value of the led indicator brightness for core 0
    QueueHandle_t core0_health_queue;
    #endif

};

#endif //__QUEUE_MANAGER_H__