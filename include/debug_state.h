#ifndef __DEBUG_STATE_H__
#define __DEBUG_STATE_H__

#include <Arduino.h>
#include <ArduinoJson.h>

// Load diagnostics that survive a crash reboot (kept in RTC memory), so the
// debug message (thing/<name>/debug) a device publishes after rebooting tells what it was
// doing when it died: which stage of loading a sequence, how much memory the
// decode took from internal RAM and PSRAM, and whether an allocation failed.
namespace debug_state
{
    enum Stage : uint8_t
    {
        STAGE_IDLE = 0,
        STAGE_FETCH,        // http request for the sequence sent
        STAGE_DECODE,       // decoding the protobuf stream into effects
        STAGE_DECODED,      // decoded, handed to the renderer
        STAGE_FIRST_FRAME,  // the renderer is rendering the first frame
        STAGE_PLAYING,      // the first frame rendered fine
    };

    // call first thing in setup(): keeps the previous boot's record and starts a new one
    void begin();

    void setStage(Stage stage, const char *triggerName = nullptr);
    void decodeStarted();
    void decodeFinished(bool success, uint32_t numEffects);
    // called by the renderer around Render() of an animation
    void frameStarted();
    void frameRendered();

    // memory now, the reset reason and the previous boot's record ("prev") and the current one ("now")
    void fillStatus(JsonObject obj);
}

#endif // __DEBUG_STATE_H__
