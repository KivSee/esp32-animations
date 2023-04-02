#include "renderer.h"

namespace esp32animations
{

    Renderer::Renderer(QueueHandle_t in_runtime_animation_queue, QueueHandle_t in_epoch_time_update_queue, QueueHandle_t in_global_brightness_queue, QueueHandle_t out_runtime_animation_queue, uint16_t number_of_leds)
        : in_runtime_animation_queue(in_runtime_animation_queue), 
            in_epoch_time_update_queue(in_epoch_time_update_queue), 
            in_global_brightness_queue(in_global_brightness_queue), 
            out_runtime_animation_queue(out_runtime_animation_queue), 
            m_number_of_leds(number_of_leds),
            m_leds_hsv(new kivsee_render::HSV[number_of_leds]),
            m_leds_rgb(number_of_leds, DATA_PIN),
            m_global_brightness(1.0)
    {
        m_leds_rgb.Begin();
    }

    void Renderer::loop(unsigned long current_millis)
    {
        readRuntimeAnimationFromQueue();
        readEpochTimeUpdateFromQueue();
        readGlobalBrightnessFromQueue();

        clear();
        if (runtime_animation.animation != nullptr)
        {
            unsigned long current_animation_time = getAnimationTime(current_millis, runtime_animation);
            if (current_animation_time)
            {
                runtime_animation.animation->Render(current_animation_time);
            }
        }
        show();
    }

    void Renderer::readRuntimeAnimationFromQueue()
    {
        RuntimeAnimation new_runtime_animation;
        if (xQueueReceive(in_runtime_animation_queue, &new_runtime_animation, 0) == pdTRUE)
        {
            Serial.print(F("[1] received new animations with "));
            Serial.print(new_runtime_animation.animation ? new_runtime_animation.animation->effects.size() : 0);
            Serial.println(F(" effects"));
            const bool animationChanged = runtime_animation.animation != new_runtime_animation.animation;
            if(animationChanged) {
                xQueueSend(out_runtime_animation_queue, &runtime_animation, 0);
            }
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

    void Renderer::readGlobalBrightnessFromQueue()
    {
        if (xQueueReceive(in_global_brightness_queue, &m_global_brightness, 0) == pdTRUE)
        {
            // m_global_brightness = new_global_brightness;
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

    void Renderer::clear()
    {
        for(int i=0; i<m_number_of_leds; i++) {
            m_leds_hsv[i].val = 0.0;
        }
    }

    void Renderer::show() {
        for(int i=0; i<m_number_of_leds; i++) {
            const kivsee_render::HSV &hsvVal = m_leds_hsv[i];
            float normalizedBrightness = hsvVal.val * hsvVal.val * m_global_brightness;
            HsbColor neoPixelColor(fmod(hsvVal.hue, 1.0f) , hsvVal.sat, normalizedBrightness);
            m_leds_rgb.SetPixelColor(i, neoPixelColor);
        }

        m_leds_rgb.Show();
    }

    kivsee_render::HSV *Renderer::hsv_painting_array() const {
        return m_leds_hsv;
    }

} // namespace esp32animations
