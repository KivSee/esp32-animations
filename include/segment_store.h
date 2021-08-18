#ifndef __SEGMENTS_STORE_H__
#define __SEGMENTS_STORE_H__

#include "hsv.h"
#include "Arduino.h"

void initSegmentStore(kivsee_render::HSV *leds);
void handleSegmentsGuidMessage(const byte *payload, unsigned int length);
void httpGetConfig();

#endif // __SEGMENTS_STORE_H__