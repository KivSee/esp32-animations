#ifndef __SECRETS_H__
#define __SECRETS_H__

#include <Arduino.h>

/*
Usage: Copy this file to include/secrets.h, and fill the values set as 'xxxx'.
*/

#define SSID "xxxx"
#define WIFI_PASSWORD "xxxx"
#define THING_NAME "xxxx"
#define MONITOR_TOPIC "monitor/" THING_NAME
#define COLOR_ORDER NeoRgbFeature   // can also be NeoGrbFeature
#define TIME_SERVER_PORT 123


#endif // __SECRETS_H__


