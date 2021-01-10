#include <Arduino.h>
#include <mqtt_manager.h>

#ifdef MQTT_BROKER_MOSQITTO
#include <mqtt_managers/mosquitto_manager.h>

MqttManager *createMqttManager(MqttManagerCallbacks *callback) {
    return new MosquittoManager(callback);
}

#elif MQTT_BROKER_GOOGLE
#include ""
#else
#error "***** ERROR ***** Neither of {Google/Mosqitto} mqtt brokers is defined ******"
#endif
