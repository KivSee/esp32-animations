#include <Arduino.h>
#include <mqtt_managers/mqtt_manager.h>

#ifdef MQTT_BROKER_MOSQITTO
#include <mqtt_managers/mosquitto_mqtt/mosquitto_manager.h>

MqttManager *createMqttManager(MqttManagerCallbacks *callback, FsManager *fsManager)
{
    return new MosquittoManager(callback);
}

#elif MQTT_BROKER_GOOGLE
#include <mqtt_managers/gcp_mqtt/gcp_manager.h>

char device_id[16];
char private_key[96];

MqttManager *createMqttManager(MqttManagerCallbacks *callback, FsManager *fsManager)
{
    bool hasThingName = fsManager->ReadThingName(device_id, 16);
    if (!hasThingName)
    {
        Serial.println("Thing name not configured - upload file to continue");
        return;
    }
    Serial.print("Thing name: ");
    Serial.println(device_id);
    // read private key
    bool hasThingKey = fsManager->ReadThingKey(private_key, 96);
    if (!hasThingKey)
    {
        Serial.println("Thing key not configured - upload file to continue");
        return;
    }
    Serial.print("Thing key: ");
    Serial.println(private_key);

    return new GcpManager(callback, device_id, private_key);
}

#else
#error "***** ERROR ***** Neither of {Google/Mosqitto} mqtt brokers is defined ******"
#endif
