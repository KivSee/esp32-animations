#ifndef __MQTT_MANAGERS_MOSQUITTO_H__
#define __MQTT_MANAGERS_MOSQUITTO_H__

#include <mqtt_managers/mqtt_manager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFiClient.h"
#include <functional>
#include <fw_version_gen.h>

#ifndef MQTT_BROKER_PORT
#define MQTT_BROKER_PORT 1883
#endif //MQTT_BROKER_PORT

const char *triggerTopic = "trigger";
const char *brightnessTopic = "brightness";

class MosquittoManager : public MqttManager
{
public:
    MosquittoManager(MqttManagerCallbacks *callback) : client(net), callback(callback) {}

public:
    void connectToMessageBroker(const char *thing_name) override
    {
        if (client.connected())
            return;
        
        char statusTopic[64];
        snprintf(statusTopic, 64, "thing/%s/status", thing_name);

        StaticJsonDocument<128> alive_will_doc;
        alive_will_doc["thingName"] = thing_name;
        alive_will_doc["alive"] = false;
        char willMsg[128];
        serializeJson(alive_will_doc, willMsg);

        client.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT); // Broker IP is defined in platformio.ini
        client.setCallback(std::bind(&MosquittoManager::mqtt_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, thing_name));
        Serial.println("connecting to mqtt");
        if (client.connect(thing_name, statusTopic, 1, true, willMsg))
        {
            Serial.println("connected to message broker");

            // publish alive message, with the firmware build so it is visible
            // (retained) which version every device is running
            StaticJsonDocument<192> alive_status_doc;
            alive_status_doc["thingName"] = thing_name;
            alive_status_doc["alive"] = true;
            alive_status_doc["fw"] = FW_BUILD;
            alive_status_doc["fw_git"] = FW_GIT;
            uint8_t aliveStatusMsg[192];
            size_t aliveStatusMsgSize = serializeJson(alive_status_doc, aliveStatusMsg);
            client.publish(statusTopic, aliveStatusMsg, aliveStatusMsgSize, true);

            // memory and load diagnostics (debug_state) go to this topic, see publish()
            snprintf(debugTopic, sizeof(debugTopic), "thing/%s/debug", thing_name);

            client.subscribe((String("animations/") + String(thing_name) + String("/#")).c_str(), 1);
            client.subscribe((String("obj/") + String(thing_name) + String("/guid")).c_str(), 1);
            // brightness before trigger: the broker delivers retained messages in
            // subscription order, so the stored brightness reaches the renderer
            // before the first animation and it never plays at the default.
            client.subscribe(brightnessTopic, 1);
            client.subscribe(triggerTopic, 1);
        }
        else
        {
            Serial.print("mqtt connect failed. error state:");
            Serial.println(client.state());
        }
    }

    // publishes to thing/<name>/debug (not retained)
    bool publish(const char *payload) override
    {
        if (!client.connected() || debugTopic[0] == '\0')
            return false;
        return client.publish(debugTopic, payload);
    }

    bool connected() override
    {
        return client.connected();
    }

    bool loop() override
    {
        return client.loop();
    }


private:
    WiFiClient net;
    PubSubClient client;
    MqttManagerCallbacks *callback;
    char debugTopic[64] = "";

    void mqtt_callback(char *topic, uint8_t* payload, unsigned int length, const char *thing_name)
    {
        Serial.print("Message arrived, ");
        Serial.print(length);
        Serial.print(" [");
        Serial.print(topic);
        Serial.print("] ");
        for (int i = 0; i < length; i++)
        {
            Serial.print((char)payload[i]);
        }
        Serial.println();

        if(strncmp("obj/", topic, 4) == 0) {
            callback->NewConfigGuidReceived(payload, length, thing_name);
        } else if(strncmp(triggerTopic, topic, sizeof(triggerTopic) + 1) == 0) {
            callback->TriggerInvoked(payload, length);
        } else if(strncmp(brightnessTopic, topic, sizeof(brightnessTopic) + 1) == 0) {
            callback->NewGlobalBrightnessReceived(payload,length);
        } else {
            Serial.println("different topic?");
        }
    }
};

#endif // __MQTT_MANAGERS_MOSQUITTO_H__