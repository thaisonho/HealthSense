#include "mqtt_manager.h"
#include "utils.h"
#include "pitches.h"

// Root CA certificate for HiveMQ Cloud
// This is the DigiCert Global Root CA used by HiveMQ Cloud
const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n" \
"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n" \
"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n" \
"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n" \
"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n" \
"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n" \
"BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n" \
"TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n" \
"DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n" \
"hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n" \
"06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n" \
"PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n" \
"YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n" \
"CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n" \
"-----END CERTIFICATE-----\n";

// Notification melody (simple short melody for the buzzer)
// Higher and clearer notes for better notification alert
const int notification_melody[] = {
    NOTE_C6, NOTE_E6, NOTE_G6, NOTE_C7,
    NOTE_G6, NOTE_E6, NOTE_C6, REST,
    NOTE_E6, NOTE_G6, NOTE_C7, REST,
    NOTE_G6, NOTE_E6, NOTE_C6, REST
};

const int notification_melody_len = sizeof(notification_melody) / sizeof(notification_melody[0]);

// Static pointer to the current instance for use in callback
static MQTTManager* currentInstance = nullptr;

// Static callback wrapper
void MQTTManager::messageCallbackWrapper(char* topic, byte* payload, unsigned int length) {
    if (currentInstance) {
        currentInstance->handleMessage(topic, payload, length);
    }
}

MQTTManager::MQTTManager(int buzzer_pin) : 
    connected(false), 
    lastReconnectAttempt(0),
    buzzerPin(buzzer_pin),
    isMeasuringCallback(nullptr)
{
    // Create the MQTT client with the secure WiFi client
    mqttClient = new PubSubClient(wifiClient);
    
    // Store instance for callback
    currentInstance = this;
    
    // Get device ID
    deviceId = DEVICE_ID;
    
    // Log buzzer pin for debugging
    Serial.print("MQTT Manager initialized with buzzer pin: ");
    Serial.println(buzzerPin);
}

MQTTManager::~MQTTManager() {
    if (mqttClient) {
        mqttClient->disconnect();
        delete mqttClient;
    }
    
    // Clear the static instance pointer if it's this instance
    if (currentInstance == this) {
        currentInstance = nullptr;
    }
}

void MQTTManager::begin() {
    // For development purposes, we can skip certificate verification
    // IMPORTANT: In production, you should use proper certificate verification
    wifiClient.setInsecure(); // Skip certificate validation
    
    // Configure MQTT client
    mqttClient->setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient->setCallback(messageCallbackWrapper);
    
    // Set a larger buffer size for MQTT messages
    mqttClient->setBufferSize(512);
    
    Serial.println("MQTT manager initialized with SSL insecure mode (dev only)");
    
    // Initial connection attempt
    connect();
}

void MQTTManager::loop() {
    // Check if connected to MQTT broker
    if (!mqttClient->connected()) {
        connected = false;
        
        // Try to reconnect periodically
        unsigned long now = millis();
        if (now - lastReconnectAttempt > reconnectInterval) {
            lastReconnectAttempt = now;
            
            // Attempt to reconnect
            if (connect()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        // Process incoming MQTT messages
        mqttClient->loop();
    }
}

bool MQTTManager::connect() {
    Serial.println("Attempting MQTT connection to HiveMQ Cloud...");
    
    // Connect to the MQTT broker with credentials and client ID
    if (mqttClient->connect(deviceId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.println("MQTT connected!");
        connected = true;
        
        // Subscribe to the device-specific topic
        subscribe();
        
        return true;
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.print(mqttClient->state());
        Serial.println(" Retrying later...");
        connected = false;
        return false;
    }
}

void MQTTManager::disconnect() {
    if (mqttClient->connected()) {
        mqttClient->disconnect();
    }
    connected = false;
}

bool MQTTManager::subscribe() {
    if (!mqttClient->connected()) {
        return false;
    }
    
    // Subscribe to the device-specific topic with QoS level 1
    String topic = deviceId;
    bool success = mqttClient->subscribe(topic.c_str(), MQTT_QOS_LEVEL);
    
    if (success) {
        Serial.print("Subscribed to topic: ");
        Serial.println(topic);
    } else {
        Serial.print("Failed to subscribe to topic: ");
        Serial.println(topic);
    }
    
    return success;
}

bool MQTTManager::publish(const char* topic, const char* message) {
    if (!mqttClient->connected()) {
        return false;
    }
    
    bool success = mqttClient->publish(topic, message);
    
    if (success) {
        Serial.print("Published to topic: ");
        Serial.print(topic);
        Serial.print(", message: ");
        Serial.println(message);
    } else {
        Serial.print("Failed to publish to topic: ");
        Serial.println(topic);
    }
    
    return success;
}

void MQTTManager::handleMessage(char* topic, byte* payload, unsigned int length) {
    // Convert payload to null-terminated string
    char* message = new char[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);
    
    // Check if the device is currently measuring
    bool isMeasuring = false;
    if (isMeasuringCallback) {
        isMeasuring = isMeasuringCallback();
    }
    
    Serial.print("Device is currently measuring: ");
    Serial.println(isMeasuring ? "YES" : "NO");
    
    // Only play notification if not currently measuring
    if (!isMeasuring) {
        Serial.println("Playing notification!");
        playNotification();
    } else {
        Serial.println("Measurement in progress - skipping notification");
    }
    
    // Free the message buffer
    delete[] message;
}

void MQTTManager::setIsMeasuringCallback(bool (*callback)()) {
    isMeasuringCallback = callback;
}

void MQTTManager::playNotification() {
    // Play the notification melody for about 8 seconds
    // The tempo divisor controls the playback speed - adjust to make it about 8 seconds
    Serial.print("Playing notification on buzzer pin: ");
    Serial.println(buzzerPin);
    
    // Use a tempo divisor of 4 for clearer, more distinct notes
    // This will make each note last longer and be more noticeable
    playMelody(buzzerPin, notification_melody, notification_melody_len, 4);
    
    Serial.println("Notification melody finished");
}
