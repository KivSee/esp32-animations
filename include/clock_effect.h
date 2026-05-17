#ifndef __CLOCK_EFFECT_H__
#define __CLOCK_EFFECT_H__

#include <effect.h>
#include <segments/segments_map.h>

// Renders the clock display for a single ring controller.
// Each controller knows its own ring index (1-12) and decides its role:
//   ring < hour  -> half brightness (past hours)
//   ring == hour -> full brightness + fractional minute fill
//   ring > hour  -> off
// All active pixels pulse once per second via a sin on brightness.
class ClockEffect : public kivsee_render::Effect
{
public:
    // ring_index: 1-based index parsed from thing_name (e.g. "ring3" -> 3)
    // esp_start_time: pointer to renderer's esp_start_time (kept live for NTP corrections)
    // hue: color hue for this ring (0.0 - 1.0)
    ClockEffect(int ring_index, const int64_t *esp_start_time, float hue,
                const kivsee_render::segments::SegmentPixels *pixels);

protected:
    void Render(float rel_time, int cycle_index) override;

private:
    int m_ring_index;
    const int64_t *m_esp_start_time;
    float m_hue;
};

#endif // __CLOCK_EFFECT_H__
