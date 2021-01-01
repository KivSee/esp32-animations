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
    };

    /*
    All the renderning stuff.
    Should run only on core 1 of the esp. 
    The only interactions are via 
    */
    class Renderer
    {
    public:
        Renderer(QueueHandle_t in_runtime_animation_queue, QueueHandle_t in_ephoc_time_update_queue, QueueHandle_t out_runtime_animation_queue, RenderUtils *render_utils);
        void loop(unsigned long current_millis);

    private:
        QueueHandle_t in_runtime_animation_queue;
        QueueHandle_t in_ephoc_time_update_queue;

        QueueHandle_t out_runtime_animation_queue;

        RenderUtils *render_utils;

    private:
        void readRuntimeAnimationFromQueue();

    private:
        RuntimeAnimation runtime_animation = {nullptr, 0};
    };

} // namespace esp32animations

#endif // __RENDERER_H__