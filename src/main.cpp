#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// Include managers
#include "wifi_manager.h"
#include "display_manager.h"
#include "sensor_manager.h"
#include "images.h"

// Define pins for the ESP32
#define SDA_PIN 21  // Default SDA pin for ESP32
#define SCL_PIN 22  // Default SCL pin for ESP32

// Define pins for ST7735 TFT display
#define TFT_CS     5
#define TFT_RST    4
#define TFT_DC     2

// Create instances
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
DisplayManager display(&tft, eva, eva_width, eva_height);
// IoT API server URL with the correct login endpoint
WiFiManager wifiManager("HealthSense", "123123123", "https://iot.newnol.io.vn");
SensorManager sensorManager(100); // buffer size 100

// App state
enum AppState {
  STATE_SETUP,
  STATE_CONNECTING,
  STATE_LOGIN,
  STATE_MEASURING
};

AppState currentState = STATE_SETUP;
bool isInitialized = false;

// Function prototypes
void setupUI();
void initializeSensor();
void updateConnectionStatus(bool connected, bool guestMode, bool loggedIn);
void sendSensorData(String uid, int32_t heartRate, int32_t spo2);

void setup() {
  Serial.begin(9600);
  
  // Initialize display
  display.begin();
  
  // Initialize sensor manager
  sensorManager.begin(SDA_PIN, SCL_PIN);
  
  // Set up callbacks for WiFi manager
  wifiManager.setSetupUICallback(setupUI);
  wifiManager.setInitializeSensorCallback(initializeSensor);
  wifiManager.setUpdateConnectionStatusCallback(updateConnectionStatus);
  wifiManager.setSendDataCallback(sendSensorData);
  
  // Set up callbacks for sensor manager
  sensorManager.setUpdateReadingsCallback([](int32_t hr, bool validHR, int32_t spo2, bool validSPO2) {
    // Always update the display with current readings and validity flags
    display.updateSensorReadings(hr, validHR, spo2, validSPO2);
    
    // Only send valid readings to the server
    if (validHR && validSPO2) {
      // Add additional validation checks using physiological constants from sensor_manager.cpp
      if (hr >= MIN_VALID_HR && hr <= MAX_VALID_HR && 
          spo2 >= MIN_VALID_SPO2 && spo2 <= MAX_VALID_SPO2) {
        Serial.println(F("Valid readings detected, sending to server"));
        wifiManager.sendSensorData(hr, spo2);
      } else {
        Serial.println(F("Readings outside physiological range, not sending"));
      }
    } else {
      Serial.println(F("Invalid readings, not sending to server"));
    }
  });
  
  sensorManager.setUpdateFingerStatusCallback([](bool fingerDetected) {
    display.showFingerStatus(fingerDetected);
  });
  
  // Begin WiFi manager (will set up AP mode)
  wifiManager.begin();
  
  currentState = STATE_SETUP;
  isInitialized = true;
}

void loop() {
  // Always process WiFi and web server
  wifiManager.loop();
  
  // State machine for app behavior
  switch (currentState) {
    case STATE_SETUP:
      // Wait for user to select connection mode
      // (handled by WiFi manager callbacks)
      break;
      
    case STATE_CONNECTING:
      // Wait for connection to establish
      // (handled by WiFi manager callbacks)
      break;
      
    case STATE_LOGIN:
      // Wait for user to log in
      // (handled by WiFi manager callbacks)
      break;
      
    case STATE_MEASURING:
      // First check if sensor is connected and working
      if (!sensorManager.checkI2CConnection()) {
        // If we're having I2C issues, show a message and wait
        static unsigned long lastErrorMsgTime = 0;
        if (millis() - lastErrorMsgTime > 5000) {  // Show error every 5 seconds
          Serial.println(F("I2C connection issues. Trying to recover..."));
          // Could update display with error message here
          lastErrorMsgTime = millis();
        }
        delay(100); // Short delay to prevent tight loop
        break;
      }
      
      // Only start sensor readings when in measuring state, sensor is ready, and we should be measuring
      if (sensorManager.isReady() && wifiManager.isMeasurementActive()) {
        static bool firstReading = true;
        
        if (firstReading) {
          display.showMeasuringStatus();
          sensorManager.readSensor();
          firstReading = false;
        } else {
          sensorManager.processReadings();
        }
      } else if (wifiManager.isMeasurementActive() && !sensorManager.isReady()) {
        // If we're supposed to be measuring but sensor isn't ready,
        // try to reinitialize it
        sensorManager.initializeSensor();
        delay(100);
      }
      break;
  }
}

// Callback for setting up UI
void setupUI() {
  display.showWiFiSetupScreen(wifiManager.getAPIP().toString().c_str(), "123123123");
}

// Callback for initializing sensor
void initializeSensor() {
  display.setupSensorUI();
  sensorManager.initializeSensor();
  sensorManager.setReady(true);
  currentState = STATE_MEASURING;
}

// Callback for connection status changes
void updateConnectionStatus(bool connected, bool guestMode, bool loggedIn) {
  if (connected && loggedIn) {
    // User mode with successful login
    display.showLoggedIn();
    currentState = STATE_MEASURING;
  } else if (guestMode) {
    // Guest mode
    display.showGuestMode();
    currentState = STATE_MEASURING;
  } else if (connected && !loggedIn) {
    // Connected but not logged in yet
    currentState = STATE_LOGIN;
    sensorManager.setReady(false);
  } else {
    // Not connected
    currentState = STATE_SETUP;
    sensorManager.setReady(false);
  }
}

// Callback for sending sensor data to server
void sendSensorData(String uid, int32_t heartRate, int32_t spo2) {
  // This function is just for logging purposes
  // The actual data sending is handled in WiFiManager's sendSensorData method
  Serial.print("Sending data for user: ");
  Serial.print(uid);
  Serial.print(", HR: ");
  Serial.print(heartRate);
  Serial.print(", SpO2: ");
  Serial.println(spo2);
}
