#ifndef __TRIGGER_H__
#define __TRIGGER_H__

#include <Arduino.h>

#include "renderer.h"

bool handleTriggerInvokedMessage(const byte *payload, unsigned int length, esp32animations::RuntimeAnimation *newTimedAnimation);


#endif // __TRIGGER_H__
