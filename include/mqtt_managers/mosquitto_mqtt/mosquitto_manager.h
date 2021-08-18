#ifndef __MQTT_MANAGERS_MOSQUITTO_H__
#define __MQTT_MANAGERS_MOSQUITTO_H__

#include <mqtt_managers/mqtt_manager.h>
#include <PubSubClient.h>
#include "WiFiClient.h"
#include <functional>

#ifndef MQTT_BROKER_PORT
#define MQTT_BROKER_PORT 1883
#endif //MQTT_BROKER_PORT

class MosquittoManager : public MqttManager
{
public:
    MosquittoManager(MqttManagerCallbacks *callback) : client(net), callback(callback) {}

public:
    void connectToMessageBroker(const char *thing_name) override
    {
        if (client.connected())
            return;

        client.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT); // Broker IP is defined in platformio.ini
        client.setCallback(std::bind(&MosquittoManager::mqtt_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        Serial.println("connecting to mqtt");
        if (client.connect(thing_name))
        {
            Serial.println("connected to message broker");
            client.subscribe((String("animations/") + String(thing_name) + String("/#")).c_str(), 1);
            client.subscribe((String("obj/") + String(thing_name) + String("/guid")).c_str(), 1);
        }
        else
        {
            Serial.print("mqtt connect failed. error state:");
            Serial.println(client.state());
        }
    }

    bool publish(const char *payload) override
    {
        return true;
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

    void mqtt_callback(char *topic, uint8_t* payload, unsigned int length)
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

        if (strncmp("animations/", topic, 11) == 0)
        {
            Serial.println("topic animation?");
            callback->NewAnimationReceived(String(topic + 11), payload, length);
        } else if(strncmp("obj/", topic, 4) == 0) {
            callback->NewConfigGuidReceived(payload, length);
        } else {
            Serial.println("different topic?");
        }
    }
};

#endif // __MQTT_MANAGERS_MOSQUITTO_H__