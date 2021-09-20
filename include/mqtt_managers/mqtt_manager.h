#ifndef MQTT_MANAGER_H_INCLUDED
#define MQTT_MANAGER_H_INCLUDED

#define MQTT_BROKER_GOOGLE 1
//#define MQTT_BROKER_MOSQITTO 1

class MqttManagerCallbacks {

    public:
        virtual void NewAnimationReceived(String triggerName, const byte *payload, unsigned int length) = 0;
        virtual void NewConfigGuidReceived(const byte *payload, unsigned int length) = 0;
        virtual void TriggerInvoked(const byte *payload, unsigned int length) = 0;

};

class MqttManager {

public:
    MqttManager() {}

public:
    virtual void connectToMessageBroker(const char * thing_name) = 0;
    virtual bool publish(const char* payload) = 0;
    virtual bool connected() = 0;
    virtual bool loop() = 0;    
};

MqttManager *createMqttManager(MqttManagerCallbacks *callback);

#endif // MQTT_MANAGER_H_INCLUDED
