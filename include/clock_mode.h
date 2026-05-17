#ifndef __CLOCK_MODE_H__
#define __CLOCK_MODE_H__

#include <animation.h>
#include <segments/segments_map.h>

// Builds a forever-running Animation that renders the clock display
// for the controller identified by thing_name (e.g. "ring3").
// esp_start_time_ptr must remain valid for the lifetime of the animation.
kivsee_render::Animation *buildClockAnimation(
    const char *thing_name,
    const int64_t *esp_start_time_ptr,
    kivsee_render::segments::SegmentsMap *segments_map);

#endif // __CLOCK_MODE_H__
