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
WiFiManager wifiManager("HealthSense", "123123123");
SensorManager sensorManager(100); // buffer size 100

// App state
enum AppState {
  STATE_SETUP,
  STATE_CONNECTING,
  STATE_MEASURING
};

AppState currentState = STATE_SETUP;
bool isInitialized = false;

// Function prototypes
void setupUI();
void initializeSensor();
void updateConnectionStatus(bool connected, bool guestMode);

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
  
  // Set up callbacks for sensor manager
  sensorManager.setUpdateReadingsCallback([](int32_t hr, bool validHR, int32_t spo2, bool validSPO2) {
    display.updateSensorReadings(hr, validHR, spo2, validSPO2);
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
      
    case STATE_MEASURING:
      // Only start sensor readings when in measuring state and sensor is ready
      if (sensorManager.isReady()) {
        static bool firstReading = true;
        
        if (firstReading) {
          display.showMeasuringStatus();
          sensorManager.readSensor();
          firstReading = false;
        } else {
          sensorManager.processReadings();
        }
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
void updateConnectionStatus(bool connected, bool guestMode) {
  if (connected) {
    display.showLoggedIn();
    currentState = STATE_MEASURING;
  } else if (guestMode) {
    display.showGuestMode();
    currentState = STATE_MEASURING;
  } else {
    currentState = STATE_SETUP;
    sensorManager.setReady(false);
  }
}
