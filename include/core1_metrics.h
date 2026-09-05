#ifndef __CORE1_METRICS_H__
#define __CORE1_METRICS_H__

#ifndef METRICS_REPORT_INTERVAL_MS
#define METRICS_REPORT_INTERVAL_MS 5000
#endif // METRICS_REPORT_INTERVAL_MS

namespace esp32animations
{
    struct Core1Metrics
    {
        unsigned int totalFrames;
        unsigned long maxFrameRenderTime;
        unsigned int numEffectsRendered;

        // --- frame timing, in microseconds ---
        // render = animation->Render() only.
        // show   = HSV->RGB conversion + the blocking NeoPixelBus push.
        // Tracked separately because only `show` scales with LED count,
        // and only `render` scales with effect count.
        unsigned long maxRenderUs;
        unsigned long maxShowUs;
        uint64_t sumRenderUs;
        uint64_t sumShowUs;
        // frames rendered within the current report window, used as the
        // divisor for the averages above and to derive FPS.
        unsigned int framesInWindow;
    };

}

#endif // __CORE1_METRICS_H__
