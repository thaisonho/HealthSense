#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include "common_types.h"


class WiFiManager {
private:
    WebServer* server;
    DNSServer* dnsServer;
    const char* ap_ssid;
    const char* ap_password;
    IPAddress apIP;
    String userSSID;
    String userPassword;
    String userEmail;
    String userUID;
    String serverURL;
    bool isConnected;
    bool isGuestMode;
    bool isLoggedIn;
    bool apModeActive;
    bool isMeasuring;
    unsigned long lastWifiCheck;
    const unsigned long wifiCheckInterval;
    int lastWifiErrorCode; // Store the last WiFi error code
    
    // CSS for web interface
    String commonCSS;
    
    // Function pointers for callbacks
    void (*setupUICallback)();
    void (*initializeSensorCallback)();
    void (*updateConnectionStatusCallback)(bool connected, bool guestMode, bool loggedIn);
    void (*sendDataCallback)(String uid, int32_t heartRate, int32_t spo2);
    void (*startNewMeasurementCallback)();
    void (*handleAIAnalysisCallback)(String summary);
    
    // Web handlers
    void handleRoot();
    void handleWiFi();
    void handleConnect();
    void handleModeSelect();
    void handleLogin();
    void handleLoginSubmit();
    void handleGuest();
    void handleMeasurement();
    void handleMeasurementInfo();
    void handleMeasurementStream(); 
    void handleStartMeasurement(); // New handler for browser to confirm page load and start measuring
    void handleCheckMeasurementStatus(); // New handler to check if measurement is complete
    void handleContinueMeasuring();
    void handleReconfigWiFi();
    void handleStatus();
    void handleForceAP();
    void handleNotFound();
    void handleAIAnalysis();
    void handleReturnToMeasurement();
    String getCommonCSS();
    
    // API communication
    bool authenticateUser(String email, String password);
    bool sendMeasurementData(String uid, int32_t heartRate, int32_t spo2);
    bool getAIHealthSummary(String& summary);

public:
    WiFiManager(const char* ap_ssid, const char* ap_password, const char* serverURL = "http://yourapiserver.com");
    void begin();
    void loop();
    void setupAPMode();
    bool connectToWiFi(String ssid, String password);
    void checkWiFiConnection();
    void readWiFiCredentials();
    void saveWiFiCredentials(String ssid, String password, bool guestMode);
    void saveUserCredentials(String email, String uid);
    void sendSensorData(int32_t heartRate, int32_t spo2);
    bool sendDeviceData(int32_t heartRate, int32_t spo2, String userId = "");
    bool requestAIHealthSummary(String& summary);
    
    // Setters for callbacks
    void setSetupUICallback(void (*callback)());
    void setInitializeSensorCallback(void (*callback)());
    void setUpdateConnectionStatusCallback(void (*callback)(bool connected, bool guestMode, bool loggedIn));
    void setHandleAIAnalysisCallback(void (*callback)(String summary));
    void setSendDataCallback(void (*callback)(String uid, int32_t heartRate, int32_t spo2));
    void setStartNewMeasurementCallback(void (*callback)());
    
    // Getters
    bool isWiFiConnected() const { return isConnected; }
    bool isInGuestMode() const { return isGuestMode; }
    bool isAPModeActive() const { return apModeActive; }
    bool isUserLoggedIn() const { return isLoggedIn; }
    bool isMeasurementActive() const { return isMeasuring; }
    String getSSID() const { return userSSID; }
    String getUserUID() const { return userUID; }
    IPAddress getAPIP() const { return apIP; }
    IPAddress getStationIP() const { return WiFi.localIP(); }
    String getConnectionInfo() const;
    int getLastWifiErrorCode() const { return lastWifiErrorCode; }
    
    // Control measurement state
    void startMeasurement();
    void stopMeasurement();
    void resetMeasurementStreamState();
    
    // WiFi stability helper
    void ensureWiFiStability();
    
    // Utility functions
    void forceAPMode();
    void restartWiFi();
    void cleanupConnections();
    void forceSocketCleanup();
};

#endif // WIFI_MANAGER_H
