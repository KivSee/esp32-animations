#include "renderer.h"

namespace esp32animations
{

    Renderer::Renderer(QueueHandle_t in_runtime_animation_queue, QueueHandle_t in_epoch_time_update_queue, QueueHandle_t out_runtime_animation_queue, RenderUtils *render_utils)
        : in_runtime_animation_queue(in_runtime_animation_queue), in_epoch_time_update_queue(in_epoch_time_update_queue), out_runtime_animation_queue(out_runtime_animation_queue), render_utils(render_utils)
    {
    }

    void Renderer::loop(unsigned long current_millis)
    {
        readRuntimeAnimationFromQueue();
        readEpochTimeUpdateFromQueue();

        render_utils->Clear();
        if (runtime_animation.animation != nullptr)
        {
            unsigned long current_animation_time = getAnimationTime(current_millis, runtime_animation);
            if (current_animation_time)
            {
                runtime_animation.animation->Render(current_animation_time);
            }
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
            updateAnimationEspStartTime(&runtime_animation);
        }
    }

    void Renderer::readEpochTimeUpdateFromQueue()
    {
        if (xQueueReceive(in_epoch_time_update_queue, &esp_start_time, 0) == pdTRUE)
        {
            updateAnimationEspStartTime(&runtime_animation);
        }
    }

    void Renderer::updateAnimationEspStartTime(RuntimeAnimation *runtime_animation)
    {
        if (runtime_animation->start_time_ms_since_epoch != 0 && esp_start_time != 0)
        {
            runtime_animation->start_time_esp_millis = runtime_animation->start_time_ms_since_epoch - esp_start_time;
        }
    }

    unsigned long Renderer::getAnimationTime(unsigned long current_millis, const RuntimeAnimation &runtime_animation)
    {
        if (!runtime_animation.start_time_esp_millis)
            return 0;
        return current_millis - runtime_animation.start_time_esp_millis;
    }

} // namespace esp32animations
