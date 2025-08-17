#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

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
    
    // Function pointers for callbacks
    void (*setupUICallback)();
    void (*initializeSensorCallback)();
    void (*updateConnectionStatusCallback)(bool connected, bool guestMode);
    
    // Web handlers
    void handleRoot();
    void handleWiFi();
    void handleConnect();
    void handleGuest();
    void handleNotFound();

public:
    WiFiManager(const char* ap_ssid, const char* ap_password);
    void begin();
    void loop();
    void setupAPMode();
    bool connectToWiFi(String ssid, String password);
    void checkWiFiConnection();
    void readWiFiCredentials();
    void saveWiFiCredentials(String ssid, String password, bool guestMode);
    
    // Setters for callbacks
    void setSetupUICallback(void (*callback)());
    void setInitializeSensorCallback(void (*callback)());
    void setUpdateConnectionStatusCallback(void (*callback)(bool connected, bool guestMode));
    
    // Getters
    bool isWiFiConnected() const { return isConnected; }
    bool isInGuestMode() const { return isGuestMode; }
    bool isAPModeActive() const { return apModeActive; }
    String getSSID() const { return userSSID; }
    IPAddress getAPIP() const { return apIP; }
};

#endif // WIFI_MANAGER_H
