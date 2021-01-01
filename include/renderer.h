#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "animation.h"
#include "render_utils.h"

namespace esp32animations
{

    struct RuntimeAnimation
    {
        kivsee_render::Animation *animation;
        unsigned long start_time_esp_millis;
        uint64_t start_time_ms_since_epoch;
    };

    /*
    All the renderning stuff.
    Should run only on core 1 of the esp. 
    The only interactions are via 
    */
    class Renderer
    {
    public:
        Renderer(QueueHandle_t in_runtime_animation_queue, QueueHandle_t in_epoch_time_update_queue, QueueHandle_t out_runtime_animation_queue, RenderUtils *render_utils);
        void loop(unsigned long current_millis);

    private:
        QueueHandle_t in_runtime_animation_queue;
        QueueHandle_t in_epoch_time_update_queue;

        QueueHandle_t out_runtime_animation_queue;

        RenderUtils *render_utils;

    private:
        void readRuntimeAnimationFromQueue();
        void readEpochTimeUpdateFromQueue();
        void updateAnimationEspStartTime(RuntimeAnimation *runtime_animation);
        unsigned long getAnimationTime(unsigned long current_millis, const RuntimeAnimation &runtime_animation);

    private:
        RuntimeAnimation runtime_animation = {nullptr, 0, 0};
        int64_t esp_start_time = 0;
    };

} // namespace esp32animations

#endif // __RENDERER_H__