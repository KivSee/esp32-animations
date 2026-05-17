#include <clock_mode.h>
#include <clock_effect.h>

#include <Arduino.h>
#include <vector>

// Parse the 1-based ring index from thing_name (e.g. "ring3" -> 3).
// Returns -1 if thing_name doesn't match the "ring<N>" pattern.
static int parseRingIndex(const char *thing_name)
{
    if (strncmp(thing_name, "ring", 4) != 0)
        return -1;
    int index = atoi(thing_name + 4);
    if (index < 1 || index > 12)
        return -1;
    return index;
}

kivsee_render::Animation *buildClockAnimation(
    const char *thing_name,
    const int64_t *esp_start_time_ptr,
    kivsee_render::segments::SegmentsMap *segments_map)
{
    int ring_index = parseRingIndex(thing_name);
    if (ring_index < 0)
    {
        Serial.print(F("clock mode: could not parse ring index from thing_name: "));
        Serial.println(thing_name);
        return nullptr;
    }

    kivsee_render::segments::SegmentPixels *pixels = segments_map->getPixelsForSegment("all");
    if (!pixels)
    {
        Serial.println(F("clock mode: 'all' segment not found in segment map"));
        return nullptr;
    }

    // One effect handles the entire clock logic for this ring.
    // Hue is fixed per role; ClockEffect decides brightness/fill at render time.
    ClockEffect *effect = new ClockEffect(ring_index, esp_start_time_ptr, 0.0f, pixels);

    std::vector<kivsee_render::Effect *> effects;
    effects.push_back(effect);

    // duration_ms=1000 means rel_time cycles every second (used by base Effect::Render),
    // but ClockEffect ignores rel_time and reads wall time directly.
    // num_repeats=0 means run forever.
    return new kivsee_render::Animation(effects.begin(), effects.end(), 1000, 0);
}
