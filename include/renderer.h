#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <NeoPixelBus.h>

#include "animation.h"
#include "runtime_animation.h"
#include "hsv.h"
#include "secrets.h"

namespace esp32animations
{

    /*
    All the renderning stuff.
    Should run only on core 1 of the esp. 
    The only interactions are via 
    */
    class Renderer
    {
    public:
        Renderer(QueueHandle_t in_runtime_animation_queue, QueueHandle_t in_epoch_time_update_queue, QueueHandle_t in_global_brightness_queue, QueueHandle_t out_runtime_animation_queue, uint16_t number_of_leds);
        void loop(unsigned long current_millis);
        kivsee_render::HSV *hsv_painting_array() const;

    private:
        void clear();
        void show();

    private:
        QueueHandle_t in_runtime_animation_queue;
        QueueHandle_t in_epoch_time_update_queue;
        QueueHandle_t in_global_brightness_queue;

        QueueHandle_t out_runtime_animation_queue;

    private:
        void readRuntimeAnimationFromQueue();
        void readEpochTimeUpdateFromQueue();
        void readGlobalBrightnessFromQueue();
        void updateAnimationEspStartTime(RuntimeAnimation *runtime_animation);
        unsigned long getAnimationTime(unsigned long current_millis, const RuntimeAnimation &runtime_animation);

    private:
        RuntimeAnimation runtime_animation = {nullptr, 0, 0};
        int64_t esp_start_time = 0;

    private:
        uint16_t m_number_of_leds;
        kivsee_render::HSV *m_leds_hsv;
        NeoPixelBus<COLOR_ORDER, Neo800KbpsMethod> m_leds_rgb;
        float m_global_brightness;
    };

} // namespace esp32animations

#endif // __RENDERER_H__