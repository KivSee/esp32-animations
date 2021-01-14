#include <Arduino.h>
#include <mqtt_managers/mqtt_manager.h>

#ifdef MQTT_BROKER_MOSQITTO
#include <mqtt_managers/mosquitto_mqtt/mosquitto_manager.h>

MqttManager *createMqttManager(MqttManagerCallbacks *callback) {
    return new MosquittoManager(callback);
}

#elif MQTT_BROKER_GOOGLE
#include <mqtt_managers/gcp_mqtt/gcp_manager.h>

MqttManager *createMqttManager(MqttManagerCallbacks *callback) {
    return new GcpManager(callback);
}

#else
#error "***** ERROR ***** Neither of {Google/Mosqitto} mqtt brokers is defined ******"
#endif
