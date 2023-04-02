#ifndef __SEQUENCE_H__
#define __SEQUENCE_H__

#include <Arduino.h>

#include "animation.h"

class SequenceManager {

    public:
        SequenceManager(QueueHandle_t runtime_animation_delete_queue);
        void loop();
        ::kivsee_render::Animation *loadSequence(const char *triggerName, uint32_t guid, const char *thing_name);
        
    private:
        ::kivsee_render::Animation *httpGetSequence(const char *triggerName, uint32_t guid, const char *thing_name);
        void deleteAnimationCache();

    private:

        QueueHandle_t runtime_animation_delete_queue;

        // cache the last values we received.
        // if we get the same message, return it from cache instead of allocating again on heap
        String m_lastTriggerName;
        uint32_t m_lastTriggerGuid;
        ::kivsee_render::Animation *m_lastDecodedAnimation;
};

#endif // __SEQUENCE_H__
