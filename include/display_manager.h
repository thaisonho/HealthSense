#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <WiFi.h>

class DisplayManager {
private:
    Adafruit_ST7735* tft;
    const uint16_t* logo;
    uint16_t logoWidth;
    uint16_t logoHeight;

public:
    // Constructor
    DisplayManager(Adafruit_ST7735* tft, const uint16_t* logo, uint16_t logoWidth, uint16_t logoHeight);
    
    // Initialize the display
    void begin();
    
    // Display functions
    void showWiFiSetupScreen(const char* apSSID, const char* apPassword);
    void showConnectionAttempt(String ssid);
    void showConnectionSuccess(String ipAddress);
    void showConnectionFailure();
    void showGuestMode();
    void showLoggedIn();
    void showLoginPage();
    void showLoginStatus(bool success);
    void setupSensorUI();
    void updateSensorReadings(int32_t heartRate, bool validHR, int32_t spo2, bool validSPO2);
    void showMeasuringStatus();
    void showFingerStatus(bool fingerDetected);
    void showWiFiReconfigOption();
    void showAIAnalysisButton();
    void showAIAnalysisLoading();
    void displayAIHealthSummary(const String& summary);
};

#endif // DISPLAY_MANAGER_H
