#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include "api_service.h"

class WiFiManager {
private:
    WebServer* server;
    DNSServer* dnsServer;
    const char* ap_ssid;
    const char* ap_password;
    IPAddress apIP;
    String userSSID;
    String userPassword;
    bool isConnected;
    bool isGuestMode;
    bool apModeActive;
    unsigned long lastWifiCheck;
    const unsigned long wifiCheckInterval;
    
    // User authentication
    String username;
    String password;
    bool isAuthenticated;
    
    // API Service
    APIService apiService;
    
    // Function pointers for callbacks
    void (*setupUICallback)();
    void (*initializeSensorCallback)();
    void (*updateConnectionStatusCallback)(bool connected, bool guestMode);
    void (*authenticationStatusCallback)(bool authenticated, String uid);
    void (*dataTransmissionCallback)(bool success);
    
    // Web handlers
    void handleRoot();
    void handleWiFi();
    void handleConnect();
    void handleGuest();
    void handleNotFound();
    void handleUserMode();
    void handleUserLogin();
    void handleLoginAttempt();

public:
    WiFiManager(const char* ap_ssid, const char* ap_password);
    void begin();
    void loop();
    void setupAPMode();
    bool connectToWiFi(String ssid, String password);
    void checkWiFiConnection();
    void readWiFiCredentials();
    void saveWiFiCredentials(String ssid, String password, bool guestMode);
    bool authenticateUser(String username, String password);
    bool sendHealthData(int heartRate, int spo2);
    
    // Setters for callbacks
    void setSetupUICallback(void (*callback)());
    void setInitializeSensorCallback(void (*callback)());
    void setUpdateConnectionStatusCallback(void (*callback)(bool connected, bool guestMode));
    void setAuthenticationStatusCallback(void (*callback)(bool authenticated, String uid));
    void setDataTransmissionCallback(void (*callback)(bool success));
    
    // Getters
    bool isWiFiConnected() const { return isConnected; }
    bool isInGuestMode() const { return isGuestMode; }
    bool isAPModeActive() const { return apModeActive; }
    String getSSID() const { return userSSID; }
    IPAddress getAPIP() const { return apIP; }
    bool isUserAuthenticated() const { return isAuthenticated; }
    String getUserId() const { return apiService.getUserId(); }
};

#endif // WIFI_MANAGER_H
