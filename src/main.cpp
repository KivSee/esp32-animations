#define CONFIG_USE_ONLY_LWIP_SELECT 1

#include <Arduino.h>
#include <WiFi.h>
#include <secrets.h>

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <TimeSync.hpp>
#include <render_utils.h>
#include <segment_store.h>
#include <pb_decode.h>
#include <animation.pb.h>
#include <effect.h>
#include <animation.h>
#include <renderer.h>
#include <mqtt_managers/mqtt_manager.h>

#ifndef NUM_LEDS
#warning NUM_LEDS not definded. using default value of 300
#define NUM_LEDS 300
#endif // NUM_LEDS

#define MAX_THING_NAME_LENGTH 16
char thing_name[MAX_THING_NAME_LENGTH] = THING_NAME;

const unsigned int WD_TIMEOUT_MS = 2000;
TimeSync::TimeSyncClient timesync;

QueueHandle_t runtime_animation_queue;
QueueHandle_t epoch_time_update_queue;
QueueHandle_t runtime_animation_delete_queue;
esp32animations::Renderer *renderer = nullptr;

// core 1 accessed
kivsee_render::HSV leds_hsv[NUM_LEDS];
std::vector<kivsee_render::HSV *> segment(NUM_LEDS);
RenderUtils renderUtils(leds_hsv, NUM_LEDS);


TaskHandle_t Task1;

void PrintCorePrefix()
{
  Serial.print("[");
  Serial.print(xPortGetCoreID());
  Serial.print("]: ");
}

void HandleTimedAnimationMsg(const byte *payload, unsigned int length)
{

  Serial.print("HandleTimedAnimationMsg(): payload: ");
  for (int i = 0; i < length; i++)
  {        
      Serial.print(payload[i], HEX);
  }
  Serial.println("");
  Serial.print("length: ");
  Serial.println(length);
  

  pb_istream_t in_stream = pb_istream_from_buffer(payload, length);

  esp32animations::RuntimeAnimation new_timed_animation;
  TimedAnimationProto timed_animation = TimedAnimationProto_init_zero;

  timed_animation.animation.funcs.decode = &kivsee_render::DecodeAnimationFromPbStream;
  timed_animation.animation.arg = &new_timed_animation.animation;

  bool success = pb_decode(&in_stream, TimedAnimationProto_fields, &timed_animation);
  if (!success)
  {
    Serial.println("failed to handle new animation msg");
    return;
  }
  if(timed_animation.start_time_ms_since_epoch) {
    new_timed_animation.start_time_esp_millis = 0;
    new_timed_animation.start_time_ms_since_epoch = timed_animation.start_time_ms_since_epoch;
  } else {
    new_timed_animation.start_time_esp_millis = millis();
    new_timed_animation.start_time_ms_since_epoch = 0;
  }

  for (int i = 0; i < NUM_LEDS; i++)
  {
    segment[i] = &leds_hsv[i];
  }
  for (::kivsee_render::Animation::EffectsVec::iterator it = new_timed_animation.animation->effects.begin(); it != new_timed_animation.animation->effects.end(); ++it)
  {
    ::kivsee_render::Effect *effect = *it;
    effect->Init(&segment);
  }

  xQueueSend(runtime_animation_queue, &new_timed_animation, portMAX_DELAY);
}


class MqttCallbacks : public MqttManagerCallbacks {

  public: 
    void NewAnimationReceived(String triggerName, const byte *payload, unsigned int length) {
      HandleTimedAnimationMsg(payload, length);
    }

};

MqttCallbacks mqttCallbacks;
MqttManager *mqttManager = createMqttManager(&mqttCallbacks);


void ConnectToWifi()
{
  if (WiFi.status() == WL_CONNECTED)
    return;

  while (true)
  {
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
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("connected to wifi");
        return;
      }
    }
    Serial.println(" could not connect for 10 seconds. retry");
  }
}

void MonitorLoop(void *parameter)
{
  initSegmentStore();
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
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
      });

  ArduinoOTA.begin();

  IPAddress ntpServerIp;
  ntpServerIp.fromString(TIME_SERVER_IP);
  timesync.updateConfiguration(15, 1000 * 60 * 10, 250, 1000 * 60 * 2);
  timesync.setup(ntpServerIp, 12321);

  unsigned int lastReportTime = millis();
  unsigned int lastMonitorTime = millis();
  for (;;)
  {
    bool isTimeChanged, isFirstClockUpdate;
    timesync.loop(&isTimeChanged, &isFirstClockUpdate);
    if (isFirstClockUpdate)
    {
      Serial.println("TIME IS NOW VALID. the esp clock was not valid and now it is");
    }
    else if (isTimeChanged)
    {
      Serial.println("TIME CHANGED. new synced clock is availible to the esp");
    }

    if(isTimeChanged || isFirstClockUpdate) {
      int64_t espStartTime = timesync.getEspStartTimeMs();
      xQueueSend(epoch_time_update_queue, &espStartTime, portMAX_DELAY);
    }

    ConnectToWifi();
    mqttManager->connectToMessageBroker(thing_name);
    unsigned int currTime = millis();
    if (currTime - lastReportTime >= 5000)
    {
      Serial.print("[0] current millis: ");
      Serial.println(millis());
      Serial.print("[0] wifi client connected: ");
      Serial.println(WiFi.status() == WL_CONNECTED);
      Serial.print("[0] mqtt client connected: ");
      Serial.println(mqttManager->connected());
      httpGetConfig();
      lastReportTime = currTime;
    }
    mqttManager->loop();

    esp32animations::RuntimeAnimation animation_from_del_q;
    if (xQueueReceive(runtime_animation_delete_queue, &animation_from_del_q, 0) == pdTRUE)
    {
      delete animation_from_del_q.animation;
    }

    ArduinoOTA.handle();

    vTaskDelay(5);
  }
}

void setup()
{
  Serial.begin(115200);
  disableCore0WDT();

  runtime_animation_queue = xQueueCreate(5, sizeof(esp32animations::RuntimeAnimation));
  epoch_time_update_queue = xQueueCreate(5, sizeof(int64_t));
  runtime_animation_delete_queue = xQueueCreate(5, sizeof(esp32animations::RuntimeAnimation));
  renderer = new esp32animations::Renderer(runtime_animation_queue, epoch_time_update_queue, runtime_animation_delete_queue, &renderUtils);

  renderUtils.Setup();

  Serial.print("Thing name: ");
  Serial.println(thing_name);
  
  xTaskCreatePinnedToCore(
      MonitorLoop,   /* Function to implement the task */
      "MonitorTask", /* Name of the task */
      16384,         /* Stack size in words */
      NULL,          /* Task input parameter */
      0,             /* Priority of the task */
      &Task1,        /* Task handle. */
      0);            /* Core where the task should run */
}

unsigned int lastPrint1Time = millis();

void loop()
{
  unsigned long current_millis = millis();

  if (current_millis - lastPrint1Time >= 5000)
  {
    Serial.println("[1] core 1 alive");
    lastPrint1Time = current_millis;
  }

  renderer->loop(current_millis);
  
  vTaskDelay(5);
}
