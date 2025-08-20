#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
// Include our fix for ESP32 and MAX30105.h compatibility
#include "esp32_max30105_fix.h"
// Make sure Wire.h is included before MAX30105.h to avoid buffer length conflicts
#include "spo2_algorithm.h"
#include "MAX30105.h"

class SensorManager {
private:
    MAX30105* particleSensor;
    uint32_t* irBuffer;    // infrared LED sensor data
    uint32_t* redBuffer;   // red LED sensor data
    int32_t bufferLength;  // data length
    int32_t spo2;          // SPO2 value
    int8_t validSPO2;      // indicator to show if the SPO2 calculation is valid
    int32_t heartRate;     // heart rate value
    int8_t validHeartRate; // indicator to show if the heart rate calculation is valid
    bool sensorReady;      // Flag indicating if sensor is ready

    // Callbacks
    void (*updateReadingsCallback)(int32_t hr, bool validHR, int32_t spo2, bool validSPO2);
    void (*updateFingerStatusCallback)(bool fingerDetected);

public:
    SensorManager(int bufferSize = 100);
    ~SensorManager();
    
    void begin(int sda_pin, int scl_pin);
    void initializeSensor();
    void readSensor();
    void processReadings();
    
    // Getters
    int32_t getHeartRate() const { return heartRate; }
    bool isHeartRateValid() const { return validHeartRate; }
    int32_t getSPO2() const { return spo2; }
    bool isSPO2Valid() const { return validSPO2; }
    bool isReady() const { return sensorReady; }
    bool isFingerDetected() const;
    
    // Set callbacks
    void setUpdateReadingsCallback(void (*callback)(int32_t hr, bool validHR, int32_t spo2, bool validSPO2));
    void setUpdateFingerStatusCallback(void (*callback)(bool fingerDetected));
    
    void setReady(bool ready) { sensorReady = ready; }
};

#endif // SENSOR_MANAGER_H
