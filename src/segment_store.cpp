
#include "segment_store.h"
#include <SPIFFS.h>
#include <HTTPClient.h>
#include "protobuf_infra.h"
#include "segments/segments_map.h"
#include "hsv.h"
#include "segments.pb.h"
#include "secrets.h"

#ifndef HTTP_SERVER_PORT
#define HTTP_SERVER_PORT 8000
#endif //HTTP_SERVER_PORT

const char *objectFileName = "/objects-config";

// data structures to use during segments map construction
kivsee_render::segments::SegmentsMap *segments_map = nullptr;
kivsee_render::HSV leds[50];

void initSegmentStore()
{
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    File file = SPIFFS.open(objectFileName);
    if (!file)
    {
        Serial.println("Failed to open objects config file for reading");
        return;
    }

    Serial.println(file.size());

    pb_istream_t pbInputStream = FileToPbStream(file);
    ::kivsee_render::segments::SegmentsMapDecodeArgs segments_map_decode_args;
    segments_map_decode_args.out_segments_map = &segments_map;
    segments_map_decode_args.leds_array = leds;
    void *arg = &segments_map_decode_args;

    // decode
    bool decodeSuccess = ::kivsee_render::segments::DecodeSegmentsMapFromPbStream(&pbInputStream, nullptr, &arg);
    if (decodeSuccess)
    {
        Serial.println("SUCCESS, segment store initialized");
        Serial.println(segments_map->guid);
        Serial.println(segments_map->number_of_pixels);
        Serial.println(segments_map->segments.size());
    }
    else
    {
        Serial.println("Failed to initialize segment store");
        Serial.println(pbInputStream.errmsg);
    }
    file.close();
}

void httpGetConfig()
{
    String httpServerAddr = "http://"; 
    httpServerAddr += HTTP_SERVER_IP; //HTTP_SERVER_IP defined in platformio.ini 
    httpServerAddr += ":";
    httpServerAddr += HTTP_SERVER_PORT;
    httpServerAddr += "/led-object/"; 
    httpServerAddr += THING_NAME; // THING_NAME defined in secrets.h
    HTTPClient http;
    http.begin(httpServerAddr.c_str());
    http.addHeader("Accept", "application/x-protobuf");
    if (segments_map) {
        http.addHeader("If-None-Match", String(segments_map->guid));
    }
    int httpResponseCode = http.GET();
    if (httpResponseCode <= 0)
    {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        http.end();
        return;
    }
    // Serial.print("HTTP Response code: ");
    // Serial.println(httpResponseCode);
    if (httpResponseCode == 304) {
        Serial.println("Object config is current, no update needed");
        http.end();
        return;
    }
    String payload = http.getString();
    Serial.println(payload.length());

    File file = SPIFFS.open(objectFileName, FILE_WRITE);
    if (!file) {
      Serial.println("There was an error opening the file for writing");
      http.end();
      return;
    }
    size_t bytesWritten = file.write((uint8_t*)payload.c_str(), payload.length());
    if(bytesWritten != payload.length()) {
      Serial.println("did not write all bytes to file");
      http.end();
      return;
    }
    file.close();
    // Free resources
    http.end();
    Serial.println("Configuration updated in FS, restarting!");
    ESP.restart();
}