#ifndef __MQTT_MANAGERS_GCP_H__
#define __MQTT_MANAGERS_GCP_H__

#include <mqtt_managers/mqtt_manager.h>
#include "WiFiClient.h"
#include <functional>

#include <Client.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <CloudIoTCore.h>
#include <CloudIoTCoreMqtt.h>
#include "./ciotc_config.h"
// #include "./Animation.h"

const String PARTY = "party";
const String WARM = "warm";
const String OFF = "off";

MqttManagerCallbacks *callback_global;

void messageReceived(String &topic, String &payload)
{
  callback_global->NewAnimationReceived("", payload.c_str(), payload.length);
  
  Serial.println("incoming: " + topic + " - " + payload);
  if (payload == PARTY)
  {
    Serial.println("Let's party!");
    // animation_party();
    // callback->NewAnimationReceived(String(topic + 11), payload, length);
  }
  else if (payload == WARM)
  {
    Serial.println("Warm & chill");
    // animation_warm();
  }
  else if (payload == OFF)
  {
    Serial.println("turning lights off");
    // animation_off();
  }
}

CloudIoTCoreDevice *device;
unsigned long iat = 0;
String jwt;

String getJwt()
{
  iat = time(nullptr);
  Serial.println("Refreshing JWT");
  jwt = device->createJWT(iat, jwt_exp_secs);
  Serial.print("---------JWT:");
  Serial.println(jwt);
  return jwt;
}

class GcpManager : public MqttManager
{
public:
  GcpManager(MqttManagerCallbacks *callback) : callback(callback)
  {
    callback_global = callback;
    device = new CloudIoTCoreDevice(
        project_id, location, registry_id, device_id,
        private_key_str);
    netClient = new WiFiClientSecure();
    client = new MQTTClient(512);
    client->setOptions(180 /* =keepAlive */, true /* =cleanSession */, 1000 /* =timeout */);
    gcp_mqtt = new CloudIoTCoreMqtt(client, netClient, device);
    gcp_mqtt->setUseLts(false);
  }

public:
  void connectToMessageBroker(const char *thing_name) override
  {
    if (connected())
      return;

    configTime(0, 0, ntp_primary, ntp_secondary);
    Serial.println("Waiting on time sync...");
    Serial.println("time:");
    Serial.println(time(nullptr));
    while (time(nullptr) < 1510644967)
    {
      delay(10);
    }

    gcp_mqtt->startMQTT();
    gcp_mqtt->mqttConnect();
  }

  bool publish(const char *payload) override
  {
    return true;
  }

  bool connected() override
  {
    return client->connected();
  }

  bool loop() override
  {

    return gcp_mqtt->loop();
    // delay(10);  // fixes some issues with WiFi stability
  }

private:
  MqttManagerCallbacks *callback;
  Client *netClient;

  CloudIoTCoreMqtt *gcp_mqtt;
  MQTTClient *client;

  // void mqtt_callback(char *topic, uint8_t* payload, unsigned int length)
  // {
  //     Serial.print("Message arrived, ");
  //     Serial.print(length);
  //     Serial.print(" [");
  //     Serial.print(topic);
  //     Serial.print("] ");
  //     for (int i = 0; i < length; i++)
  //     {
  //         Serial.print((char)payload[i]);
  //     }
  //     Serial.println();

  //     // if (strncmp("animations/", topic, 11) == 0)
  //     // {
  //     //     Serial.println("topic animation?");
  //     //     callback->NewAnimationReceived(String(topic + 11), payload, length);
  //     // } else {
  //     //     Serial.println("different topic?");
  //     // }
  // }

  bool publishTelemetry(String data)
  {
    return gcp_mqtt->publishTelemetry(data);
  }

  bool publishTelemetry(const char *data, int length)
  {
    return gcp_mqtt->publishTelemetry(data, length);
  }

  bool publishTelemetry(String subfolder, String data)
  {
    return gcp_mqtt->publishTelemetry(subfolder, data);
  }

  // bool publishRaw(String subfolder, String data){
  // return gcp_mqtt->publishRaw(subfolder, data);
  // }

  bool publishTelemetry(String subfolder, const char *data, int length)
  {
    return gcp_mqtt->publishTelemetry(subfolder, data, length);
  }
};

#endif // __MQTT_MANAGERS_GCP_H__