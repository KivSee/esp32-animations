#ifndef __SEGMENTS_STORE_H__
#define __SEGMENTS_STORE_H__

#include "hsv.h"
#include "Arduino.h"
#include "segments/segments_map.h"

void initSegmentStore(kivsee_render::HSV *leds);
void handleSegmentsGuidMessage(const byte *payload, unsigned int length);
void httpGetConfig();
kivsee_render::segments::SegmentsMap *getSegmentsMap();

#endif // __SEGMENTS_STORE_H__