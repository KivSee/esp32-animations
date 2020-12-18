#define CONFIG_USE_ONLY_LWIP_SELECT 1

#include <Arduino.h>
#include <WiFi.h>
#include <secrets.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <render_utils.h>
// #include <protobuf_infra.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <effect.h>
#include <animation.h>

#ifndef NUM_LEDS
#warning NUM_LEDS not definded. using default value of 300
#define NUM_LEDS 300
#endif // NUM_LEDS

#ifndef MQTT_BROKER_PORT
#define MQTT_BROKER_PORT 1883
#endif //MQTT_BROKER_PORT

#ifndef MONITOR_TOPIC_PREFIX
#define MONITOR_TOPIC_PREFIX "monitor"
#endif // MONITOR_TOPIC_PREFIX
String monitorTopic(MONITOR_TOPIC_PREFIX "/");

#define MAX_THING_NAME_LENGTH 16
char thing_name[MAX_THING_NAME_LENGTH] = THING_NAME;

const unsigned int WD_TIMEOUT_MS = 2000;

// core 1 accessed
kivsee_render::Animation *animation = nullptr;
kivsee_render::HSV leds_hsv[NUM_LEDS];
std::vector<kivsee_render::HSV *> segment(NUM_LEDS);
RenderUtils renderUtils(leds_hsv, NUM_LEDS);

TaskHandle_t Task1;

QueueHandle_t effectQueue;
const int effectQueueSize = 10;

QueueHandle_t effectDelQueue;
const int effectDelQueueSize = 10;

void PrintCorePrefix()
{
  Serial.print("["); Serial.print(xPortGetCoreID()); Serial.print("]: ");
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {

  Serial.print("Message arrived, ");
  Serial.print(length);
  Serial.print(" [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strncmp("animations/", topic, 11) == 0) {
    // uint8_t buff[100] = {10, 35, 58, 33, 10, 7, 18, 5, 21, 0, 0, 128, 63, 21, 0, 0, 128, 63, 26, 12, 34, 10, 13, 0, 0, 160, 64, 21, 0, 0, 128, 62, 37, 0, 0, 128, 63, 18, 7, 10, 5, 13, 0, 0, 128, 63};
    // pb_istream_t in_stream = pb_istream_from_buffer(buff, sizeof(buff));
    pb_istream_t in_stream = pb_istream_from_buffer(payload, length);
    kivsee_render::Animation *new_animation = nullptr;
    void *arg = &new_animation;
    bool success = kivsee_render::DecodeAnimationFromPbStream(&in_stream, nullptr, &arg);
    if(!success) {
      Serial.println("failed to handle new animation msg");
      return;
    }

    for (int i = 0; i < NUM_LEDS; i++)
    {
        segment[i] = &leds_hsv[i];
    }
    for(::kivsee_render::Animation::EffectsVec::iterator it = new_animation->effects.begin(); it != new_animation->effects.end(); ++it) {
        ::kivsee_render::Effect *effect = *it;
        effect->Init(&segment);
    }

    xQueueSend(effectQueue, &new_animation, portMAX_DELAY);
  }
  // if (strncmp("animations/", topic, 11) == 0) {
  //   int songNameStartIndex = 11 + strlen(thing_name) + 1;
  //   String songName = String(topic + songNameStartIndex);
  //   fsManager.SaveToFs((String("/music/") + songName).c_str(), payload, length);

  //   if(songOffsetTracker.GetCurrentFile() == songName) {
  //     SendAnListUpdate();
  //   }
    
  // } else if(strcmp("current-song", topic) == 0) {
  //   songOffsetTracker.HandleCurrentSongMessage((char *)payload);
  //   SendAnListUpdate();

  // } else if(strncmp("objects-config", topic, 14) == 0) {
  //   fsManager.SaveToFs("/objects-config", payload, length);
  //   ESP.restart();
  // }

  Serial.print("[0] done handling mqtt callback: ");
  Serial.println(topic);
}

void ConnectToWifi() {

  if (WiFi.status() == WL_CONNECTED)
    return;

  while(true) {
    unsigned int connectStartTime = millis();
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, WIFI_PASSWORD);
    Serial.printf("Attempting to connect to SSID: ");
    Serial.printf(SSID);
    while (millis() - connectStartTime < 10000)
    {
        Serial.print(".");
        delay(1000);
        if(WiFi.status() == WL_CONNECTED) {
          Serial.println("connected to wifi");
          return;
        }
    }
    Serial.println(" could not connect for 10 seconds. retry");
  }
}

WiFiClient net;
PubSubClient client(net);
void ConnectToMessageBroker() {
    if(client.connected())
      return;

    client.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT); // Broker IP is defined in platformio.ini
    client.setCallback(mqtt_callback);
    // StaticJsonDocument<128> json_doc;
    // json_doc["ThingName"] = thing_name;
    // json_doc["Alive"] = false;
    char lastWillMsg[16] = "0";
    // serializeJson(json_doc, lastWillMsg);
    Serial.println("connecting to mqtt");
    if(client.connect(thing_name, monitorTopic.c_str(), 1, true, lastWillMsg)) {
        Serial.println("connected to message broker");
        // client.subscribe((String("objects-config/") + String(thing_name)).c_str(), 1);
        // client.subscribe("current-song", 1);
        client.subscribe((String("animations/") + String(thing_name) + String("/#")).c_str(), 1);
    }
    else {
        Serial.print("mqtt connect failed. error state:");
        Serial.println(client.state());
    }
}


void MonitorLoop( void * parameter) {

  ConnectToWifi();

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname(thing_name);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  unsigned int lastReportTime = millis();
  unsigned int lastMonitorTime = millis();
  for(;;) {
    ConnectToWifi();
    ConnectToMessageBroker();
    unsigned int currTime = millis();
    if (currTime - lastMonitorTime >= 1000) {
      char monitorMsg[16] = "1";
      client.publish(monitorTopic.c_str(), monitorMsg, true);
      lastMonitorTime = currTime;
    }
    if(currTime - lastReportTime >= 5000) {
      Serial.print("[0] current millis: ");
      Serial.println(millis());
      Serial.print("[0] wifi client connected: ");
      Serial.println(WiFi.status() == WL_CONNECTED);
      Serial.print("[0] mqtt client connected: ");
      Serial.println(client.connected());
      lastReportTime = currTime;
    }
    client.loop();

    kivsee_render::Animation *animation_from_del_q;
    if(xQueueReceive(effectDelQueue, &animation_from_del_q, 0) == pdTRUE) {
      delete animation_from_del_q;
    }

    ArduinoOTA.handle();

    vTaskDelay(5);
  }
}


void setup() {
  Serial.begin(115200);
  disableCore0WDT();

  effectQueue = xQueueCreate( effectQueueSize, sizeof(kivsee_render::Animation *) );
  effectDelQueue = xQueueCreate( effectDelQueueSize, sizeof(kivsee_render::Animation *) );

  renderUtils.Setup();

  Serial.print("Thing name: "); Serial.println(thing_name);
  monitorTopic += thing_name;
  Serial.print("Mqtt monitor topic is: "); Serial.println(monitorTopic);

  xTaskCreatePinnedToCore(
      MonitorLoop, /* Function to implement the task */
      "MonitorTask", /* Name of the task */
      16384,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &Task1,  /* Task handle. */
      0); /* Core where the task should run */

}

unsigned int lastPrint1Time = millis();

void loop() {
  unsigned long currentMillis = millis();
  kivsee_render::Animation *animation_from_q;

  if(currentMillis - lastPrint1Time >= 5000) {
    Serial.println("[1] core 1 alive");
    lastPrint1Time = currentMillis;
  }

  if(xQueueReceive(effectQueue, &animation_from_q, 0) == pdTRUE) {
    xQueueSend(effectDelQueue, &animation, 0);
    animation = animation_from_q;
  }

  renderUtils.Clear();
  if (animation != nullptr) {
    animation->Render(currentMillis);
  }
  renderUtils.Show();

  vTaskDelay(5);

}

