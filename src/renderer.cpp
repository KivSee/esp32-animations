#include "renderer.h"

namespace esp32animations
{

    Renderer::Renderer(QueueHandle_t in_runtime_animation_queue, QueueHandle_t in_ephoc_time_update_queue, QueueHandle_t out_runtime_animation_queue, RenderUtils *render_utils)
        : in_runtime_animation_queue(in_runtime_animation_queue), in_ephoc_time_update_queue(in_ephoc_time_update_queue), out_runtime_animation_queue(out_runtime_animation_queue), render_utils(render_utils)
    {
    }

    void Renderer::loop(unsigned long current_millis)
    {
        readRuntimeAnimationFromQueue();

        render_utils->Clear();
        if (runtime_animation.animation != nullptr)
        {
            unsigned long current_animation_time = current_millis - runtime_animation.start_time_esp_millis;
            runtime_animation.animation->Render(current_animation_time);
        }
        render_utils->Show();
    }

    void Renderer::readRuntimeAnimationFromQueue()
    {
        RuntimeAnimation new_runtime_animation;
        if (xQueueReceive(in_runtime_animation_queue, &new_runtime_animation, 0) == pdTRUE)
        {
            xQueueSend(out_runtime_animation_queue, &runtime_animation, 0);
            runtime_animation = new_runtime_animation;
        }
    }

} // namespace esp32animations
