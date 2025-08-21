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
  STATE_MEASURING,
  STATE_AI_ANALYSIS
};

AppState currentState = STATE_SETUP;
bool isInitialized = false;
String aiSummaryResult = "";

// Function prototypes
void setupUI();
void initializeSensor();
void updateConnectionStatus(bool connected, bool guestMode, bool loggedIn);
void sendSensorData(String uid, int32_t heartRate, int32_t spo2);
void handleAIAnalysisRequest(String summaryText);

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
  wifiManager.setStartNewMeasurementCallback([]() {
    Serial.println(F("Starting new measurement from web interface..."));
    if (sensorManager.isReady()) {
      sensorManager.startMeasurement();
    }
  });
  
  // Set up callbacks for sensor manager
  sensorManager.setUpdateReadingsCallback([](int32_t hr, bool validHR, int32_t spo2, bool validSPO2) {
    // Always update the display with current readings and validity flags
    display.updateSensorReadings(hr, validHR, spo2, validSPO2);
    
    // Log current readings for monitoring
    if (validHR && validSPO2) {
      Serial.print(F("Current valid reading: HR="));
      Serial.print(hr);
      Serial.print(F(", SpO2="));
      Serial.println(spo2);
    }
  });
  
  sensorManager.setUpdateFingerStatusCallback([](bool fingerDetected) {
    display.showFingerStatus(fingerDetected);
    
    // Start measurement when finger is detected (if not already measuring)
    if (fingerDetected && !sensorManager.isMeasurementInProgress() && 
        wifiManager.isMeasurementActive()) {
      Serial.println(F("👆 Finger detected, starting measurement..."));
      Serial.print(F("📊 Measurement states - Sensor measuring: "));
      Serial.print(sensorManager.isMeasurementInProgress() ? "YES" : "NO");
      Serial.print(F(", WiFi measurement active: "));
      Serial.println(wifiManager.isMeasurementActive() ? "YES" : "NO");
      sensorManager.startMeasurement();
    } else if (fingerDetected && sensorManager.isMeasurementInProgress()) {
      Serial.println(F("👆 Finger detected but measurement already in progress"));
    } else if (fingerDetected && !wifiManager.isMeasurementActive()) {
      Serial.println(F("👆 Finger detected but no measurement requested from web interface"));
    }
    
    // If finger is removed during measurement, warn but continue measuring
    if (!fingerDetected && sensorManager.isMeasurementInProgress()) {
      Serial.print(F("⚠️  Finger removed during measurement! Progress: "));
      Serial.print(sensorManager.getValidReadingCount());
      Serial.print(F("/"));
      Serial.print(REQUIRED_VALID_READINGS);
      Serial.println(F(" - Please keep finger on sensor"));
    }
  });
  
  // Set callback for when measurement is complete (5 valid readings collected)
  sensorManager.setMeasurementCompleteCallback([](int32_t avgHR, int32_t avgSpO2) {
    Serial.println(F("=== MEASUREMENT COMPLETE CALLBACK ==="));
    Serial.print(F("Final averaged HR: "));
    Serial.println(avgHR);
    Serial.print(F("Final averaged SpO2: "));
    Serial.println(avgSpO2);
    
    // Update display with final results
    display.updateSensorReadings(avgHR, true, avgSpO2, true);
    
    // Send final averaged data to server (only if in user mode and logged in)
    wifiManager.sendSensorData(avgHR, avgSpO2);
    
    Serial.println(F("Measurement cycle complete. Sensor stopped."));
    Serial.println(F("Press 'Start New Measurement' to measure again."));
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
      
      // Only process sensor readings when in measuring state, sensor is ready, and we should be measuring
      static bool initialReadingDone = false;
      
      if (sensorManager.isReady() && wifiManager.isMeasurementActive()) {
        
        if (!initialReadingDone) {
          display.showMeasuringStatus();
          sensorManager.readSensor();
          initialReadingDone = true;
        }
        
        // Always continue processing readings for continuous measurement
        // This will keep collecting samples until we have 5 valid readings
        sensorManager.processReadings();
        
      } else {
        // Reset flag when measurement is not active
        initialReadingDone = false;
        
        if (wifiManager.isMeasurementActive() && !sensorManager.isReady()) {
          // If we're supposed to be measuring but sensor isn't ready,
          // try to reinitialize it
          sensorManager.initializeSensor();
          delay(100);
        }
      }
      break;
      
    case STATE_AI_ANALYSIS:
      // Just display the AI analysis - user will return via web interface
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
  
  // Start a new measurement cycle when sensor is initialized
  Serial.println(F("Sensor initialized, ready for measurement when finger is detected"));
  
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

// Handle AI analysis request with provided summary text
void handleAIAnalysisRequest(String summaryText) {
  // Update the current state
  currentState = STATE_AI_ANALYSIS;
  
  // Display the AI health summary
  display.displayAIHealthSummary(summaryText);
  Serial.println("AI Health Summary displayed");
}
