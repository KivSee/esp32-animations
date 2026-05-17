#ifndef __SEQUENCE_H__
#define __SEQUENCE_H__

#include <Arduino.h>

#include "animation.h"
#include "segments/segments_map.h"

class SequenceManager {

    public:
        SequenceManager(QueueHandle_t runtime_animation_queue, QueueHandle_t runtime_animation_delete_queue);
        void loop();
        void handleTriggerInvokedMessage(const byte *payload, unsigned int length, const char *thing_name);

        // Must be called before clock triggers can be handled.
        void setClockDependencies(const int64_t *esp_start_time_ptr,
                                  kivsee_render::segments::SegmentsMap *segments_map);

    private:
        ::kivsee_render::Animation *loadSequence(const char *triggerName, uint32_t guid, const char *thing_name);
        ::kivsee_render::Animation *httpGetSequence(const char *triggerName, uint32_t guid, const char *thing_name);
        ::kivsee_render::Animation *buildClock(const char *thing_name);
        void deleteRuntimeAnimation(kivsee_render::Animation *animationToDelete);
        bool hasCachedValue() { return m_lastDecodedAnimation != nullptr; }
        bool waitForMemoryReclame(uint maxMsToWait);
        void sendEmptyAnimationToRenderer();

    private:

        QueueHandle_t m_runtime_animation_queue;
        QueueHandle_t m_runtime_animation_delete_queue;

        // clock mode dependencies (set via setClockDependencies)
        const int64_t *m_esp_start_time_ptr = nullptr;
        kivsee_render::segments::SegmentsMap *m_segments_map = nullptr;

        // cache the last values we received.
        // if we get the same message, return it from cache instead of allocating again on heap
        String m_lastTriggerName;
        uint32_t m_lastTriggerGuid;
        ::kivsee_render::Animation *m_lastDecodedAnimation = nullptr;
};

#endif // __SEQUENCE_H__
