#ifndef __RUNTIME_ANIMATION_H__
#define __RUNTIME_ANIMATION_H__

#include "animation.h"

namespace esp32animations
{

    struct RuntimeAnimation
    {
        // the animation ptr holds a pointer ro an animations object
        // which has all the effects to render with thier configuration.
        // it is created and deleted on core 0 and consumed by core 1
        kivsee_render::Animation *animation;

        unsigned long start_time_esp_millis;
        
        uint64_t start_time_ms_since_epoch;
    };

}

#endif // __RUNTIME_ANIMATION_H__