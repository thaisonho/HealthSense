#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
// Include our fix for ESP32 and MAX30105.h compatibility
#include "esp32_max30105_fix.h"
// Wire.h is included before MAX30105.h to avoid buffer length conflicts
#include "spo2_algorithm.h"
#include "MAX30105.h"

// Constants for sensor configuration
#define LED_BRIGHTNESS_DEFAULT 50      // Default brightness level (0-255)
#define LED_BRIGHTNESS_LOW 0x1A        // Lower brightness to prevent saturation
#define LED_BRIGHTNESS_VERY_LOW 0x15   // Very low brightness for extreme cases
#define SAMPLE_AVERAGE 4               // Number of samples to average
#define LED_MODE_SPO2 2                // Mode 2 = RED + IR for SpO2
#define SAMPLE_RATE 100                // 100Hz sample rate
#define PULSE_WIDTH 411                // Maximum pulse width for sensitivity
#define ADC_RANGE 4096                 // Default ADC range

// Constants for signal processing
#define MIN_VALID_HR 40                // Minimum physiologically valid heart rate
#define MAX_VALID_HR 220               // Maximum physiologically valid heart rate
#define MIN_VALID_SPO2 70              // Minimum physiologically valid SpO2
#define MAX_VALID_SPO2 100             // Maximum physiologically valid SpO2
#define IR_SIGNAL_THRESHOLD 20000      // Threshold for IR signal detection
#define RED_SIGNAL_THRESHOLD 15000     // Threshold for red signal detection
#define SIGNAL_SATURATION_LIMIT 350000 // Upper limit suggesting sensor saturation (increased for stronger signals)
#define REQUIRED_VALID_READINGS 5      // Number of valid readings required before averaging
#define MEASUREMENT_TIMEOUT_MS 120000   // Maximum time to wait for 5 valid readings (120 seconds - longer for I2C recovery)

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
    unsigned long lastI2CErrorTime; // Last time an I2C error occurred
    int i2cErrorCount;     // Count of consecutive I2C errors
    int sda_pin;           // SDA pin for I2C
    int scl_pin;           // SCL pin for I2C
    
    // Measurement averaging system
    int32_t validReadings[REQUIRED_VALID_READINGS][2]; // Store [HR, SpO2] pairs
    int validReadingCount; // Current count of valid readings
    bool isMeasuring;      // Whether measurement is in progress
    int32_t averagedHR;    // Final averaged heart rate
    int32_t averagedSpO2;  // Final averaged SpO2
    bool measurementComplete; // Flag indicating measurement is complete
    unsigned long measurementStartTime; // Time when measurement started
    
    // Callbacks
    void (*updateReadingsCallback)(int32_t hr, bool validHR, int32_t spo2, bool validSPO2);
    void (*updateFingerStatusCallback)(bool fingerDetected);
    void (*measurementCompleteCallback)(int32_t avgHR, int32_t avgSpO2);

public:
    SensorManager(int bufferSize = 100);
    ~SensorManager();
    
    void begin(int sda_pin, int scl_pin);
    void initializeSensor();
    void readSensor();
    void processReadings();
    void resetSensor();
    bool checkI2CConnection();
    
    // Getters
    int32_t getHeartRate() const { return heartRate; }
    bool isHeartRateValid() const { return validHeartRate; }
    int32_t getSPO2() const { return spo2; }
    bool isSPO2Valid() const { return validSPO2; }
    bool isReady() const { return sensorReady; }
    bool isFingerDetected() const;
    
    // Measurement control
    void startMeasurement();
    void stopMeasurement();
    bool isMeasurementInProgress() const { return isMeasuring; }
    bool isMeasurementReady() const { return measurementComplete; }
    int32_t getAveragedHR() const { return averagedHR; }
    int32_t getAveragedSpO2() const { return averagedSpO2; }
    int getValidReadingCount() const { return validReadingCount; }
    
    // Set callbacks
    void setUpdateReadingsCallback(void (*callback)(int32_t hr, bool validHR, int32_t spo2, bool validSPO2));
    void setUpdateFingerStatusCallback(void (*callback)(bool fingerDetected));
    void setMeasurementCompleteCallback(void (*callback)(int32_t avgHR, int32_t avgSpO2));
    
    void setReady(bool ready) { sensorReady = ready; }
};

#endif // SENSOR_MANAGER_H
