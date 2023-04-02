#include "queue_manager.h"

#include "runtime_animation.h"

QueueManager::QueueManager() : runtime_animation_queue(xQueueCreate(5, sizeof(esp32animations::RuntimeAnimation))),
                               epoch_time_update_queue(xQueueCreate(5, sizeof(int64_t))),
                               global_brightness_queue(xQueueCreate(5, sizeof(float))),
                               runtime_animation_delete_queue(xQueueCreate(5, sizeof(esp32animations::RuntimeAnimation)))
{
}
