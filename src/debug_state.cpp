#include <debug_state.h>

#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

namespace debug_state
{
    static const uint32_t MAGIC = 0x4B495644; // "KIVD"

    struct LoadRecord
    {
        uint32_t magic;
        uint32_t bootCount;
        uint8_t stage;
        char trigger[24];
        uint32_t numEffects;
        uint8_t decodeOk;
        uint32_t internalFreeBefore;
        uint32_t internalLargestBefore;
        uint32_t psramFreeBefore;
        int32_t internalUsed;
        int32_t psramUsed;
        uint32_t failedAllocs;
        uint32_t lastFailedSize;
        uint32_t lastFailedCaps;
        uint32_t frames;
        uint32_t uptimeMs;
    };

    // not cleared on a software reset, panic or watchdog reset (garbage after power on, hence the magic)
    static RTC_NOINIT_ATTR LoadRecord rtcRecord;
    static LoadRecord prevRecord;
    static bool hasPrev = false;

    static void onFailedAlloc(size_t size, uint32_t caps, const char *functionName)
    {
        rtcRecord.failedAllocs++;
        rtcRecord.lastFailedSize = size;
        rtcRecord.lastFailedCaps = caps;
    }

    static const char *stageName(uint8_t stage)
    {
        switch (stage)
        {
        case STAGE_IDLE: return "idle";
        case STAGE_FETCH: return "fetch";
        case STAGE_DECODE: return "decode";
        case STAGE_DECODED: return "decoded";
        case STAGE_FIRST_FRAME: return "first_frame";
        case STAGE_PLAYING: return "playing";
        default: return "?";
        }
    }

    static const char *resetReasonName(esp_reset_reason_t reason)
    {
        switch (reason)
        {
        case ESP_RST_POWERON: return "poweron";
        case ESP_RST_SW: return "sw";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "int_wdt";
        case ESP_RST_TASK_WDT: return "task_wdt";
        case ESP_RST_WDT: return "wdt";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_DEEPSLEEP: return "deepsleep";
        default: return "other";
        }
    }

    void begin()
    {
        uint32_t bootCount = 0;
        if (rtcRecord.magic == MAGIC)
        {
            prevRecord = rtcRecord;
            prevRecord.trigger[sizeof(prevRecord.trigger) - 1] = '\0';
            hasPrev = true;
            bootCount = rtcRecord.bootCount;
        }
        memset(&rtcRecord, 0, sizeof(rtcRecord));
        rtcRecord.magic = MAGIC;
        rtcRecord.bootCount = bootCount + 1;
        heap_caps_register_failed_alloc_callback(&onFailedAlloc);

        Serial.printf("[debug] boot %u, reset reason: %s\n", rtcRecord.bootCount, resetReasonName(esp_reset_reason()));
        if (hasPrev)
        {
            Serial.printf("[debug] previous boot: stage %s, trigger '%s', %u effects, decode %s, internal used %d, psram used %d, failed allocs %u (last %u bytes), frames %u, uptime %u ms\n",
                          stageName(prevRecord.stage), prevRecord.trigger, prevRecord.numEffects,
                          prevRecord.decodeOk ? "ok" : "not finished/failed", prevRecord.internalUsed, prevRecord.psramUsed,
                          prevRecord.failedAllocs, prevRecord.lastFailedSize, prevRecord.frames, prevRecord.uptimeMs);
        }
    }

    void setStage(Stage stage, const char *triggerName)
    {
        rtcRecord.stage = stage;
        rtcRecord.uptimeMs = millis();
        if (triggerName)
        {
            strncpy(rtcRecord.trigger, triggerName, sizeof(rtcRecord.trigger) - 1);
            rtcRecord.trigger[sizeof(rtcRecord.trigger) - 1] = '\0';
            rtcRecord.numEffects = 0;
            rtcRecord.decodeOk = 0;
            rtcRecord.internalUsed = 0;
            rtcRecord.psramUsed = 0;
            rtcRecord.frames = 0;
        }
    }

    void decodeStarted()
    {
        rtcRecord.internalFreeBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        rtcRecord.internalLargestBefore = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        rtcRecord.psramFreeBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        setStage(STAGE_DECODE);
    }

    void decodeFinished(bool success, uint32_t numEffects)
    {
        rtcRecord.decodeOk = success ? 1 : 0;
        rtcRecord.numEffects = numEffects;
        rtcRecord.internalUsed = (int32_t)rtcRecord.internalFreeBefore - (int32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        rtcRecord.psramUsed = (int32_t)rtcRecord.psramFreeBefore - (int32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        setStage(success ? STAGE_DECODED : STAGE_IDLE);
    }

    void frameStarted()
    {
        if (rtcRecord.stage == STAGE_DECODED)
            rtcRecord.stage = STAGE_FIRST_FRAME;
    }

    void frameRendered()
    {
        rtcRecord.frames++;
        rtcRecord.uptimeMs = millis();
        if (rtcRecord.stage == STAGE_FIRST_FRAME)
            rtcRecord.stage = STAGE_PLAYING;
    }

    static void fillRecord(JsonObject obj, const LoadRecord &record)
    {
        obj["stage"] = stageName(record.stage);
        obj["trig"] = record.trigger;
        obj["fx"] = record.numEffects;
        obj["dec_ok"] = record.decodeOk;
        obj["int_before"] = record.internalFreeBefore;
        obj["int_blk_before"] = record.internalLargestBefore;
        obj["ps_before"] = record.psramFreeBefore;
        obj["int_used"] = record.internalUsed;
        obj["ps_used"] = record.psramUsed;
        obj["alloc_fail"] = record.failedAllocs;
        obj["alloc_fail_size"] = record.lastFailedSize;
        obj["alloc_fail_caps"] = record.lastFailedCaps;
        obj["frames"] = record.frames;
        obj["up_ms"] = record.uptimeMs;
    }

    void fillStatus(JsonObject obj)
    {
        obj["boot"] = rtcRecord.bootCount;
        obj["reset"] = resetReasonName(esp_reset_reason());
        obj["ps_total"] = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
        obj["ps_free"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        obj["int_total"] = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
        obj["int_free"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        obj["int_min"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        obj["int_blk"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        if (hasPrev)
            fillRecord(obj.createNestedObject("prev"), prevRecord);
        fillRecord(obj.createNestedObject("now"), rtcRecord);
    }
}
