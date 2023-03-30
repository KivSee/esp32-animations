#include <trigger.h>

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>

#include <sequence.h>

SequenceManager sequenceManager;

bool handleTriggerInvokedMessage(const byte *payload, unsigned int length, esp32animations::RuntimeAnimation *newTimedAnimation, const char *thing_name)
{
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return false;
    }

    const char *triggerName = doc["trigger_name"].as<const char *>();
    uint32_t guid = doc["guid"].as<uint32_t>();
    uint64_t startTimeMsSinceEpoch = doc["start_time_ms_since_epoch"].as<uint64_t>();

    char buf[200];
    snprintf(buf, sizeof(buf), "got trigger: %s. guid: %d, start time: %lld", triggerName ? triggerName : "NONE", guid, startTimeMsSinceEpoch);
    Serial.println(buf);

    newTimedAnimation->start_time_ms_since_epoch = startTimeMsSinceEpoch;
    newTimedAnimation->start_time_esp_millis = 0;

    if(!triggerName) {
        Serial.println("no active trigger");
        newTimedAnimation->animation = nullptr;
    } else {
        newTimedAnimation->animation = sequenceManager.loadSequence(triggerName, guid, thing_name);
        if(newTimedAnimation->animation == nullptr) {
            return false;
        }
    }

    return true;
}