#include <sequence.h>

#include <HTTPClient.h>
#include <pb_decode.h>

#include <animation.h>

#include "secrets.h"
#include "protobuf_infra.h"
#include "segment_store.h"

::kivsee_render::Animation *httpGetSequence(const char *triggerName, uint32_t guid, const char *thing_name)
{
    char uri[128];
    int uriLen = snprintf(uri, sizeof(uri), "/triggers/%s/objects/%s/guid/%lu", triggerName, thing_name, guid);
    if (uriLen < 0 || uriLen >= sizeof(uri))
    {
        Serial.println("cannot format seq uri");
        return nullptr;
    }

    Serial.print(F("fetching sequence from uri: "));
    Serial.println(uri);

    uint16_t port = (uint16_t)strtoul(LED_SEQ_SERVICE_PORT, nullptr, 10);
    if(port == 0) {
        Serial.println(F("could not parse sequence service port"));
        return nullptr;
    }

    HTTPClient http;
    http.begin(LED_SEQ_SERVICE_IP, port, uri);
    http.addHeader("Accept", "application/x-protobuf");

    int httpResponseCode = http.GET();
    if (httpResponseCode <= 0)
    {
        Serial.print(F("sequence service GET error code: "));
        Serial.println(httpResponseCode);
        http.end();
        return nullptr;
    }

    if (httpResponseCode >= 400)
    {
        Serial.println(F("failed to GET led sequence from service"));
        http.end();
        return nullptr;
    }

    Stream *httpStream = http.getStreamPtr();
    int payloadSize = http.getSize();
    pb_istream_t nanopbStream = StreamToPbStream(httpStream, payloadSize);

    kivsee_render::DecodeAnimationArgs args = {
        getSegmentsMap()
    };
    void *decodeArgs = &args;

    bool decodeSuccess = kivsee_render::DecodeAnimationFromPbStream(&nanopbStream, nullptr, &decodeArgs);
    if(!decodeSuccess) {
        Serial.print(F("failed to decode sequence proto. error: "));
        Serial.println(nanopbStream.errmsg);
        http.end();
        return nullptr;
    }

    ::kivsee_render::Animation *animation = (::kivsee_render::Animation *)decodeArgs;
    Serial.print(F("successfully decoded sequence from protobuf. found "));
    Serial.print(animation->effects.size());
    Serial.println(F(" effects"));

    http.end();

    return animation;
}

::kivsee_render::Animation *loadSequence(const char *triggerName, uint32_t guid, const char *thing_name)
{
    // we used to have another option here to read from FS as a fast caching,
    // but the FS write were sooooo slow (~5 seconds)
    // and until the data was saved to FS we did not render.
    // so it was removed in order to not block the starting of new trigger.
    //
    // when we have lots of controllers, this might overload the network / service,
    // which will need to be tested and verified to work properly
    return httpGetSequence(triggerName, guid, thing_name);
}
