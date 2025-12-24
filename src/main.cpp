#define CONFIG_USE_ONLY_LWIP_SELECT 1

#include <Arduino.h>
#include <WiFi.h>
#include <secrets.h>
#include <cstring>

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>

#include <segment_store.h>
#include <sequence.h>
#include <brightness.h>
#include <pb_decode.h>
#include <kivsee/proto/render/v1/animation.pb.h>
#include <effect.h>
#include <animation.h>
#include <renderer.h>
#include <mqtt_managers/mqtt_manager.h>
#include <fs_manager.h>
#include <time_manager.h>
#include <queue_manager.h>
#include <metrics.h>

#define MAX_THING_NAME_LENGTH 16
char thing_name[MAX_THING_NAME_LENGTH];

const unsigned int WD_TIMEOUT_MS = 2000;

const QueueManager queueManager;
esp32animations::Renderer *renderer = nullptr; // initialize after we read num pixels
SequenceManager sequenceManager(queueManager.runtime_animation_queue, queueManager.runtime_animation_delete_queue);
FsManager fsManager;
TimeManager timeManager(queueManager.epoch_time_update_queue);
Metrics metrics(queueManager.core1_metrics_queue);

unsigned int lastWiFiCheckTime = 0;
unsigned int lastOTAHandleTime = 0;
unsigned int lastReportTime = 0;

class MqttCallbacks : public MqttManagerCallbacks
{

public:

  void NewConfigGuidReceived(const byte *payload, unsigned int length, const char *thing_name)
  {
    handleSegmentsGuidMessage(payload, length, thing_name);
  }

  void TriggerInvoked(const byte *payload, unsigned int length)
  {
    // copy payload into trigger queue to avoid doing HTTP/decode work inside
    // the MQTT callback (which can block the mqtt loop)
    QueueManager::TriggerMessage msg;
    if (length > sizeof(msg.payload)) length = sizeof(msg.payload);
    msg.length = (uint16_t)length;
    memcpy(msg.payload, payload, msg.length);
    xQueueSend(queueManager.trigger_queue, &msg, 0);
  }

  void NewGlobalBrightnessReceived(const byte *payload, unsigned int length)
  {
    float new_global_brightness;
    bool success = handleGlobalBrightnessMessage(payload, length, &new_global_brightness);
    if (success) {
      // Use timeout instead of portMAX_DELAY to avoid blocking indefinitely
      xQueueSend(queueManager.global_brightness_queue, &new_global_brightness, pdMS_TO_TICKS(100));
    }
  }
};

MqttCallbacks mqttCallbacks;
MqttManager *mqttManager;

void ConnectToWifi()
{
  if (WiFi.status() == WL_CONNECTED)
    return;

  // Non-blocking WiFi connection attempt
  // Only start connection if not already attempting
  static unsigned int connectStartTime = 0;
  static bool connecting = false;

  if (!connecting) {
    connectStartTime = millis();
    connecting = true;
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, WIFI_PASSWORD);
    Serial.printf("Attempting to connect to SSID: ");
    Serial.println(SSID);
  }

  // Check connection status without blocking
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("connected to wifi");
    connecting = false;
    httpGetConfig(thing_name);
    return;
  }

  // If connection attempt times out, reset and try again next loop
  if (millis() - connectStartTime >= 10000)
  {
    Serial.println(" could not connect for 10 seconds. retry");
    connecting = false;
  }
}

// MonitorLoop removed - was for dual-core ESP32 setup, no longer needed on ESP32-C3

void setup()
{
  Serial.begin(115200);

  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  bool hasThingName = fsManager.ReadThingName(thing_name, 16);
  while (!hasThingName)
  {
    String str = "no name";
    strcpy(thing_name, str.c_str());
    Serial.println("Thing name not configured - upload 'thing_info' file to continue");
    delay(5000);
  }
  Serial.print("Thing name: "); Serial.println(thing_name);
  metrics.setup(thing_name);

  uint16_t number_of_leds = readNumberOfPixels();
  if(number_of_leds == 0) {
    number_of_leds = 300;
  }

  renderer = new esp32animations::Renderer(queueManager, number_of_leds);

  initSegmentStore(renderer->hsv_painting_array(), number_of_leds);

  fsManager.setup();

  mqttManager = createMqttManager(&mqttCallbacks, &fsManager);

  ConnectToWifi();
  ArduinoOTA.setHostname(thing_name);
  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else
          type = "filesystem";
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
  timeManager.begin();
}

unsigned int lastPrint1Time = millis();

void loop()
{
  unsigned long current_millis = millis();
  unsigned long loop_start = millis();
  unsigned long task_start;

  // WiFi connection check (every 10s)
  if (current_millis - lastWiFiCheckTime >= 10000)
  {
    task_start = millis();
    ConnectToWifi();
    unsigned long wifi_time = millis() - task_start;
    if (wifi_time > 100) Serial.printf("[DEBUG] ConnectToWifi: %lu ms\n", wifi_time);

    task_start = millis();
    mqttManager->connectToMessageBroker(thing_name);
    unsigned long mqtt_connect_time = millis() - task_start;
    if (mqtt_connect_time > 100) Serial.printf("[DEBUG] MQTT connect: %lu ms\n", mqtt_connect_time);

    lastWiFiCheckTime = current_millis;
  }

  // OTA handle
  task_start = millis();
  ArduinoOTA.handle();
  unsigned long ota_time = millis() - task_start;
  if (ota_time > 100) Serial.printf("[DEBUG] OTA handle: %lu ms\n", ota_time);

  // Status reporting (every 5s)
  if (current_millis - lastReportTime >= 5000)
  {
    Serial.print("[0] current millis: ");
    Serial.println(millis());
    Serial.print("[0] wifi client connected: ");
    Serial.println(WiFi.status() == WL_CONNECTED);
    Serial.print("[0] mqtt client connected: ");
    Serial.println(mqttManager->connected());
    Serial.print("[0] rssi: ");
    Serial.println(WiFi.RSSI());
    lastReportTime = current_millis;
  }

  // Metrics, time sync, MQTT, sequence management
  task_start = millis();
  metrics.loop();
  unsigned long metrics_time = millis() - task_start;
  if (metrics_time > 100) Serial.printf("[DEBUG] metrics.loop: %lu ms\n", metrics_time);

  task_start = millis();
  timeManager.loop();
  unsigned long time_mgr_time = millis() - task_start;
  if (time_mgr_time > 100) Serial.printf("[DEBUG] timeManager.loop: %lu ms\n", time_mgr_time);

  task_start = millis();
  mqttManager->loop();
  unsigned long mqtt_loop_time = millis() - task_start;
  if (mqtt_loop_time > 100) Serial.printf("[DEBUG] mqttManager->loop: %lu ms\n", mqtt_loop_time);

  // Drain trigger queue and handle triggers outside of MQTT callback to avoid
  // blocking the mqtt loop with HTTP/decoding work.
  // Limit processing to prevent blocking the main loop for too long
  QueueManager::TriggerMessage trigMsg;
  unsigned int triggerProcessStart = millis();
  int triggerCount = 0;
  const int MAX_TRIGGERS_PER_LOOP = 1; // Process max 1 trigger per loop to avoid blocking
  while (xQueueReceive(queueManager.trigger_queue, &trigMsg, 0) == pdTRUE && triggerCount < MAX_TRIGGERS_PER_LOOP) {
    sequenceManager.handleTriggerInvokedMessage(trigMsg.payload, trigMsg.length, thing_name);
    triggerCount++;
    // If processing takes too long, break to allow other operations
    if (millis() - triggerProcessStart > 100) {
      Serial.println("[WARN] Trigger processing taking too long, deferring remaining triggers");
      break;
    }
  }

  task_start = millis();
  sequenceManager.loop();
  unsigned long seq_time = millis() - task_start;
  if (seq_time > 100) Serial.printf("[DEBUG] sequenceManager.loop: %lu ms\n", seq_time);

  // Renderer loop
  if(renderer) {
    task_start = millis();
    renderer->loop(current_millis);
    unsigned long render_time = millis() - task_start;
    if (render_time > 100) Serial.printf("[DEBUG] renderer.loop: %lu ms\n", render_time);
  }

  // Report total loop time if it exceeds threshold
  unsigned long total_loop_time = millis() - loop_start;
  if (total_loop_time > 500) {
    Serial.printf("[DEBUG] === TOTAL LOOP TIME: %lu ms ===\n", total_loop_time);
  }

  // Feed watchdog timer to prevent resets during long operations
  // Note: ESP32-C3 has built-in watchdog, but we should yield regularly
  vTaskDelay(10);

  // Yield to other tasks and allow WiFi stack to process
  yield();
}
