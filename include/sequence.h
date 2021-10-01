#ifndef __SEQUENCE_H__
#define __SEQUENCE_H__

#include <Arduino.h>

#include "animation.h"

::kivsee_render::Animation *loadSequence(const char *triggerName, uint32_t guid);

#endif // __SEQUENCE_H__
