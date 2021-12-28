#define CONFIG_USE_ONLY_LWIP_SELECT 1

#include <Arduino.h>
#include <WiFi.h>
#include <secrets.h>

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>

#include <TimeSync.hpp>
#include <segment_store.h>
#include <trigger.h>
#include <pb_decode.h>
#include <animation.pb.h>
#include <effect.h>
#include <animation.h>
#include <renderer.h>
#include <mqtt_managers/mqtt_manager.h>
#include <fs_manager.h>

#define MAX_THING_NAME_LENGTH 16
char thing_name[MAX_THING_NAME_LENGTH] = THING_NAME;

const unsigned int WD_TIMEOUT_MS = 2000;
TimeSync::TimeSyncClient timesync;

QueueHandle_t runtime_animation_queue;
QueueHandle_t epoch_time_update_queue;
QueueHandle_t runtime_animation_delete_queue;
esp32animations::Renderer *renderer = nullptr;
FsManager fsManager;

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
  kivsee_render::DecodeAnimationArgs args = {
      getSegmentsMap()};
  timed_animation.animation.arg = &args;

  bool success = pb_decode(&in_stream, TimedAnimationProto_fields, &timed_animation);
  if (!success)
  {
    Serial.print("failed to handle new animation msg. error: ");
    Serial.println(in_stream.errmsg);
    return;
  }

  Serial.println("succesfully initialized new animation");

  new_timed_animation.animation = (::kivsee_render::Animation *)timed_animation.animation.arg;

  if (timed_animation.start_time_ms_since_epoch)
  {
    new_timed_animation.start_time_esp_millis = 0;
    new_timed_animation.start_time_ms_since_epoch = timed_animation.start_time_ms_since_epoch;
  }
  else
  {
    new_timed_animation.start_time_esp_millis = millis();
    new_timed_animation.start_time_ms_since_epoch = 0;
  }

  xQueueSend(runtime_animation_queue, &new_timed_animation, portMAX_DELAY);
}

class MqttCallbacks : public MqttManagerCallbacks
{

public:
  void NewAnimationReceived(String triggerName, const byte *payload, unsigned int length)
  {
    HandleTimedAnimationMsg(payload, length);
  }

  void NewConfigGuidReceived(const byte *payload, unsigned int length)
  {
    handleSegmentsGuidMessage(payload, length);
  }

  void TriggerInvoked(const byte *payload, unsigned int length)
  {
    esp32animations::RuntimeAnimation new_timed_animation = {};
    bool success = handleTriggerInvokedMessage(payload, length, &new_timed_animation);
    xQueueSend(runtime_animation_queue, &new_timed_animation, portMAX_DELAY);
  }
};

MqttCallbacks mqttCallbacks;
MqttManager *mqttManager;

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
        httpGetConfig();
        return;
      }
    }
    Serial.println(" could not connect for 10 seconds. retry");
  }
}

void MonitorLoop(void *parameter)
{
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
  Serial.print("Time sync server IP: ");
  Serial.println(TIME_SERVER_IP);
  ntpServerIp.fromString(TIME_SERVER_IP);
  timesync.updateConfiguration(15, 1000 * 60 * 10, 250, 1000 * 60 * 2);
  timesync.setup(ntpServerIp, 12321);

  unsigned int lastReportTime = millis();
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

    if (isTimeChanged || isFirstClockUpdate)
    {
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

  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  disableCore0WDT();

  uint16_t number_of_leds = readNumberOfPixels();
  if(number_of_leds == 0) {
    return;
  }

  runtime_animation_queue = xQueueCreate(5, sizeof(esp32animations::RuntimeAnimation));
  epoch_time_update_queue = xQueueCreate(5, sizeof(int64_t));
  runtime_animation_delete_queue = xQueueCreate(5, sizeof(esp32animations::RuntimeAnimation));
  renderer = new esp32animations::Renderer(runtime_animation_queue, epoch_time_update_queue, runtime_animation_delete_queue, number_of_leds);
  initSegmentStore(renderer->hsv_painting_array(), number_of_leds);

  fsManager.setup();

  mqttManager = createMqttManager(&mqttCallbacks, &fsManager);

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

  if(renderer) {
    renderer->loop(current_millis);
  }

  vTaskDelay(5);
}
