#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "common_types.h"

// MQTT Configuration
#define MQTT_BROKER       "70030b8b8dc741c79d6ab7ffa586f461.s1.eu.hivemq.cloud"
#define MQTT_PORT         8883
#define MQTT_USERNAME     "phamngocthai"
#define MQTT_PASSWORD     "Thai2005"
#define MQTT_QOS_LEVEL    1

// Forward declaration
class SensorManager;

class MQTTManager {
private:
    WiFiClientSecure wifiClient;
    PubSubClient* mqttClient;
    String deviceId;
    bool connected;
    unsigned long lastReconnectAttempt;
    const int reconnectInterval = 5000; // 5 seconds between reconnect attempts
    int buzzerPin;
    
    // Callback function pointer for measuring status check
    bool (*isMeasuringCallback)();
    
    // Static callback wrapper for MQTT message callback
    static void messageCallbackWrapper(char* topic, byte* payload, unsigned int length);
    
    // Actual message handler (instance method)
    void handleMessage(char* topic, byte* payload, unsigned int length);
    
public:
    MQTTManager(int buzzer_pin);
    ~MQTTManager();
    
    void begin();
    void loop();
    bool connect();
    void disconnect();
    bool isConnected() const { return connected && mqttClient->connected(); }
    
    // MQTT operations
    bool subscribe();
    bool publish(const char* topic, const char* message);
    
    // Set the callback to check if device is measuring
    void setIsMeasuringCallback(bool (*callback)());
    
    // Notification handling
    void playNotification();
};

#endif // MQTT_MANAGER_H
