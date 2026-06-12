#include <clock_effect.h>

#include <Arduino.h>
#include <cmath>
#include <time.h>


ClockEffect::ClockEffect(int ring_index, const int64_t *esp_start_time, float hue,
                         const kivsee_render::segments::SegmentPixels *pixels)
    : m_ring_index(ring_index),
      m_esp_start_time(esp_start_time),
      m_hue(hue)
{
    // Bypass Effect::Init() — set protected fields directly.
    // start=0, end=ULONG_MAX so the base class Render() never gates us out.
    this->segment_pixels = pixels;
    this->start_time     = 0;
    this->end_time       = ULONG_MAX;
    this->repeat_num     = 0.0f;
    this->repeat_start   = 0.0f;
    this->repeat_end     = 1.0f;
}

void ClockEffect::Render(float /*rel_time*/, int /*cycle_index*/)
{
    // Derive wall-clock time from esp_start_time + millis().
    // esp_start_time is the epoch-ms when millis() was 0.
    if (!m_esp_start_time || *m_esp_start_time == 0)
        return;

    int64_t epoch_ms = *m_esp_start_time + (int64_t)millis();
    time_t epoch_s   = (time_t)(epoch_ms / 1000);

    struct tm t;
    localtime_r(&epoch_s, &t);

    int hour12 = t.tm_hour % 12; // 0–11 (noon/midnight = 0)
    int minute  = t.tm_min;       // 0-59
    int second  = t.tm_sec;       // 0-59

    // Determine this ring's role.
    // ring_index 1..12, hour12 1..12
    // ring < hour12  -> hour ring (half brightness)
    // ring == hour12 -> minute ring (full brightness, fractional fill by minute)
    // ring > hour12  -> off

    int minute_ring = hour12 + 1; // rings 1..hour12 are hour rings, hour12+1 is minute ring

    if (m_ring_index > minute_ring)
    {
        // off - leave pixels at 0 (clear() already zeroed them)
        return;
    }

    // Pulse: sin over 2-second period, range [0.5, 1.0]
    float rel_cycle       = (float)(epoch_ms % 2000) / 2000.0f;
    float pulse           = 0.5f + 0.5f * sinf(rel_cycle * 2.0f * M_PI - M_PI / 2.0f);
    float brightness_base = 0.5f + 0.5f * pulse; // range [0.5, 1.0]

    if (m_ring_index < minute_ring)
    {
        // Hour ring: hue = 1.0 / ring_index, half brightness pulsing
        float hue        = (float)m_ring_index / 12.0f;
        float brightness = 0.5f * brightness_base;
        for (auto &px : *segment_pixels)
        {
            px.pixel->hue = hue;
            px.pixel->sat = 1.0f;
            px.pixel->val = brightness;
        }
    }
    else // m_ring_index == minute_ring
    {
        // 12 sub-rings × 5 minutes each = 60 minutes total.
        // Full sub-rings = minute / 5, partial pixels within next sub-ring = (minute % 5) / 5 * 12.
        int full_sub_rings   = minute / 5;
        int partial_pixels   = (int)roundf((float)(minute % 5) / 5.0f * 12.0f);
        int fill_pixel_count = full_sub_rings * 12 + partial_pixels;

        // Fill order starts at the sub-ring sitting at 1 o'clock on the face and walks
        // counter-clockwise. Ring 12 is mounted with sub-ring 9 at 1 o'clock, and each
        // ring after it (ring 1, 2, ...) is rotated clockwise by another 30°, so on
        // ring N the sub-ring at 1 o'clock is (9 - N) mod 12.
        // For a hardware sub-ring s, its fill-order sub-ring is (s - anchor + 12) % 12, so
        //   fill_index = ((s - anchor + 12) % 12) * 12 + pos.
        // The snake lives in the sub-ring whose fill_index range starts at full_sub_rings * 12.
        static const int   SNAKE_TAIL   = 3;
        static const float SNAKE_PERIOD = 12.0f; // pixels per cycle
        int   anchor_sub       = ((9 - m_ring_index) % 12 + 12) % 12;
        int   snake_fill_start = full_sub_rings * 12;
        float head_pos         = rel_cycle * SNAKE_PERIOD; // 0.0 → 12.0

        float brightness = 1.0f * brightness_base;
        for (auto &px : *segment_pixels)
        {
            int pixel_index = (int)roundf(px.relativePositionInSegment * 143.0f);
            int hw_sub      = pixel_index / 12;
            int pos         = pixel_index % 12;
            int fill_sub    = (hw_sub - anchor_sub + 12) % 12;
            int fill_index  = fill_sub * 12 + pos;

            if (fill_index < fill_pixel_count)
            {
                px.pixel->hue = fill_index / 143.0f;
                px.pixel->sat = 1.0f;
                px.pixel->val = brightness;
            }
            else if (fill_index >= snake_fill_start && fill_index < snake_fill_start + 12)
            {
                int snake_pos = fill_index - snake_fill_start; // 0–11 within snake sub-ring
                // Cyclic distance behind head (wrapping over 12 pixels)
                float dist = fmodf(head_pos - (float)snake_pos + 12.0f, 12.0f);
                if (dist <= (float)SNAKE_TAIL)
                {
                    float snake_brightness = (1.0f - dist / (float)SNAKE_TAIL) * brightness;
                    px.pixel->hue = fill_index / 143.0f;
                    px.pixel->sat = 1.0f;
                    px.pixel->val = snake_brightness;
                }
            }
        }
    }
}
