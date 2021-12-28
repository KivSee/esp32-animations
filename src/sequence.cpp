#include <sequence.h>

#include <SPIFFS.h>
#include <HTTPClient.h>
#include <pb_decode.h>

#include <animation.h>

#include "secrets.h"
#include "protobuf_infra.h"
#include "segment_store.h"

#define MAX_FILE_NAME_LEN 31
const char *etagHeaderKey = "etag";

// on the FS, the sequence is stored at /seq/${trigger_name}.data
// and the guid is stored on /seq/${trigger_name}.guid

void deleteCurrentSequenceFiles(const char *guidFileName, const char *dataFileName)
{
    SPIFFS.remove(guidFileName);
    SPIFFS.remove(dataFileName);
}

bool writeGuidFile(const char *guidFileName, uint32_t guid)
{
    File guidFile = SPIFFS.open(guidFileName, FILE_WRITE);
    if (!guidFile)
    {
        Serial.println(F("Sequence saving: there was an error opening the guid file for writing"));
        return false;
    }
    const size_t bytesWrittenGuid = guidFile.write((const uint8_t *)&guid, sizeof(guid));
    if (bytesWrittenGuid != sizeof(guid))
    {
        Serial.println(F("Sequence saving: could not write guid to file"));
        guidFile.close();
        return false;
    }
    guidFile.close();
    return true;
}

bool httpGetSequence(const char *triggerName, uint32_t guid, const char *guidFileName, const char *dataFileName, const char *thing_name)
{

    deleteCurrentSequenceFiles(guidFileName, dataFileName);

    char uri[128];
    int uriLen = snprintf(uri, sizeof(uri), "/triggers/%s/objects/%s/guid/%lu", triggerName, thing_name, guid);
    if (uriLen < 0 || uriLen >= sizeof(uri))
    {
        Serial.println("cannot format seq uri");
        return false;
    }

    uint16_t port = (uint16_t)strtoul(LED_SEQ_SERVICE_PORT, nullptr, 10);
    if(port == 0) {
        Serial.println("could not parse sequence service port");
        return false;
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
        return false;
    }

    if (httpResponseCode >= 400)
    {
        Serial.println(F("failed to GET led sequence from service"));
        http.end();
        return false;
    }

    if (!writeGuidFile(guidFileName, guid))
    {
        http.end();
        return false;
    }

    File dataFile = SPIFFS.open(dataFileName, FILE_WRITE);
    if (!dataFile)
    {
        Serial.println(F("Sequence saving: there was an error opening the data file for writing"));
        http.end();
        return false;
    }

    int bytesWritten = http.writeToStream(&dataFile);
    if (bytesWritten < 0 || bytesWritten != http.getSize())
    {
        Serial.println(F("Sequence saving: did not write all bytes to file"));
        dataFile.close();
        http.end();
        return false;
    }

    // Free resources
    dataFile.close();
    http.end();
    Serial.println(F("Got sequence from service"));

    return true;
}

::kivsee_render::Animation *loadSequence(const char *triggerName, uint32_t guid, const char *thing_name)
{

    char guidFileName[MAX_FILE_NAME_LEN + 1];
    const int guidFileNameLen = snprintf(guidFileName, sizeof(guidFileName), "/seq/%s.guid", triggerName);
    if (guidFileNameLen < 0 || guidFileNameLen >= sizeof(guidFileName))
    {
        Serial.println(F("trigger name too long to create spiffs guid filename"));
        return nullptr;
    }

    char dataFileName[MAX_FILE_NAME_LEN + 1];
    const int dataFileNameLen = snprintf(dataFileName, sizeof(dataFileName), "/seq/%s.data", triggerName);
    if (dataFileNameLen < 0 || dataFileNameLen >= sizeof(dataFileName))
    {
        Serial.println(F("trigger name too long to create spiffs data filename"));
        return nullptr;
    }

    File guidFile = SPIFFS.open(guidFileName, "r");
    if (!guidFile || guidFile.available() == 0)
    {
        if (!httpGetSequence(triggerName, guid, guidFileName, dataFileName, thing_name))
        {
            return nullptr;
        }
    }
    else
    {
        uint32_t storedGuid;
        size_t bytesRead = guidFile.read((uint8_t *)&storedGuid, sizeof(storedGuid));
        if (bytesRead != sizeof(storedGuid))
        {
            Serial.println(F("Sequence read: could not read guid"));
            return nullptr;
        }

        if (storedGuid != guid)
        {
            Serial.println(F("Sequence new guid detected, fetching sequence from service"));
            if (!httpGetSequence(triggerName, guid, guidFileName, dataFileName, thing_name))
            {
                return nullptr;
            }
        }
        Serial.println(F("sequence found on FS, no need to fetch it"));
    }
    guidFile.close();

    File dataFile = SPIFFS.open(dataFileName, "r");
    if (!dataFile || dataFile.available() == 0)
    {
        Serial.println(F("sequence data could not be found in file"));
        dataFile.close();
        return nullptr;
    }
    pb_istream_t pbInputStream = FileToPbStream(dataFile);
    kivsee_render::DecodeAnimationArgs args = {
        getSegmentsMap()
    };
    void *decodeArgs = &args;

    bool decodeSuccess = kivsee_render::DecodeAnimationFromPbStream(&pbInputStream, nullptr, &decodeArgs);
    if(!decodeSuccess) {
        Serial.print(F("failed to decode sequence proto. error: "));
        Serial.println(pbInputStream.errmsg);
        return nullptr;
    }

    ::kivsee_render::Animation *animation = (::kivsee_render::Animation *)decodeArgs;
    Serial.println(F("successfully decoded sequence from protobuf"));
    dataFile.close();

    return animation;
}
