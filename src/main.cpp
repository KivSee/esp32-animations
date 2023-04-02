#define CONFIG_USE_ONLY_LWIP_SELECT 1

#include <Arduino.h>
#include <WiFi.h>
#include <secrets.h>

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>
#include <InfluxDbClient.h>

#include <TimeSync.hpp>
#include <segment_store.h>
#include <sequence.h>
#include <brightness.h>
#include <pb_decode.h>
#include <animation.pb.h>
#include <effect.h>
#include <animation.h>
#include <renderer.h>
#include <mqtt_managers/mqtt_manager.h>
#include <fs_manager.h>

#define MAX_THING_NAME_LENGTH 16
char thing_name[MAX_THING_NAME_LENGTH];

const unsigned int WD_TIMEOUT_MS = 2000;
TimeSync::TimeSyncClient timesync;

QueueHandle_t runtime_animation_queue;
QueueHandle_t epoch_time_update_queue;
QueueHandle_t global_brightness_queue;
QueueHandle_t runtime_animation_delete_queue;
esp32animations::Renderer *renderer = nullptr;
SequenceManager *sequenceManager = nullptr;
FsManager fsManager;

TaskHandle_t Task1;

#define INFLUXDB_URL "http://" INFLUXDB_IP ":" INFLUXDB_PORT
#define INFLUXDB_DB_NAME "kivsee"
InfluxDBClient influxClient(INFLUXDB_URL, INFLUXDB_DB_NAME);
Point sensor("wifi_status");

void PrintCorePrefix()
{
  Serial.print("[");
  Serial.print(xPortGetCoreID());
  Serial.print("]: ");
}

class MqttCallbacks : public MqttManagerCallbacks
{

public:

  void NewConfigGuidReceived(const byte *payload, unsigned int length, const char *thing_name)
  {
    handleSegmentsGuidMessage(payload, length, thing_name);
  }

  void TriggerInvoked(const byte *payload, unsigned int length)
  {
    sequenceManager->handleTriggerInvokedMessage(payload, length, thing_name);
  }

  void NewGlobalBrightnessReceived(const byte *payload, unsigned int length)
  {
    float new_global_brightness;
    bool success = handleGlobalBrightnessMessage(payload, length, &new_global_brightness);
    if (success) {
      xQueueSend(global_brightness_queue, &new_global_brightness, portMAX_DELAY);
    }
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
        httpGetConfig(thing_name);
        return;
      }
    }
    Serial.println(" could not connect for 10 seconds. retry");
  }
}

void MonitorLoop(void *parameter)
{

  bool hasThingName = fsManager.ReadThingName(thing_name, 16);
  while (!hasThingName)
  {
    String str = "no name";
    strcpy(thing_name, str.c_str()); 
    Serial.println("Thing name not configured - upload 'thing_info' file to continue");
    delay(5000);
  }
  Serial.print("Thing name: "); Serial.println(thing_name);

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

  sensor.addTag("device", thing_name);

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
      Serial.println("TIME CHANGED. new synced clock is available to the esp");
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
      Serial.print("[0] rssi: ");
      Serial.println(WiFi.RSSI());
      lastReportTime = currTime;

      sensor.clearFields();
      // Report RSSI of currently connected network
      sensor.addField("rssi", WiFi.RSSI());
      sensor.addField("uptime", millis());
      sensor.addField("free heap", esp_get_free_heap_size());
      // Print what are we exactly writing
      Serial.print("Writing: ");
      Serial.println(sensor.toLineProtocol()); 
      // Write point
      if (!influxClient.writePoint(sensor)) {
        Serial.print("InfluxDB write failed: ");
        Serial.println(influxClient.getLastErrorMessage());
      }
    }
    mqttManager->loop();
    sequenceManager->loop();

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
    number_of_leds = 300;
  }

  runtime_animation_queue = xQueueCreate(5, sizeof(esp32animations::RuntimeAnimation));
  epoch_time_update_queue = xQueueCreate(5, sizeof(int64_t));
  global_brightness_queue = xQueueCreate(5, sizeof(float));
  runtime_animation_delete_queue = xQueueCreate(5, sizeof(esp32animations::RuntimeAnimation));

  renderer = new esp32animations::Renderer(runtime_animation_queue, epoch_time_update_queue, global_brightness_queue, runtime_animation_delete_queue, number_of_leds);
  sequenceManager = new SequenceManager(runtime_animation_queue, runtime_animation_delete_queue);

  initSegmentStore(renderer->hsv_painting_array(), number_of_leds);

  fsManager.setup();

  mqttManager = createMqttManager(&mqttCallbacks, &fsManager);

  xTaskCreatePinnedToCore(
      MonitorLoop,   /* Function to implement the task */
      "MonitorTask", /* Name of the task */
      8192,          /* Stack size in words */
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
