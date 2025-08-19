#include "wifi_manager.h"
#include <EEPROM.h>

// EEPROM constants
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 64
#define MODE_ADDR 128

WiFiManager::WiFiManager(const char* ap_ssid, const char* ap_password) :
    ap_ssid(ap_ssid),
    ap_password(ap_password),
    isConnected(false),
    isGuestMode(false),
    apModeActive(false),
    lastWifiCheck(0),
    wifiCheckInterval(5000),
    username(""),
    password(""),
    isAuthenticated(false),
    apiService("http://localhost:30000/api"),
    setupUICallback(nullptr),
    initializeSensorCallback(nullptr),
    updateConnectionStatusCallback(nullptr),
    authenticationStatusCallback(nullptr),
    dataTransmissionCallback(nullptr)
{
    apIP = IPAddress(192, 168, 4, 1);
    server = new WebServer(80);
    dnsServer = new DNSServer();
}

void WiFiManager::begin() {
    // Read saved WiFi credentials
    readWiFiCredentials();
    
    // Always start in AP mode first to allow WiFi selection
    setupAPMode();
    
    // Setup web server
    server->on("/", [this](){ this->handleRoot(); });
    server->on("/wifi", [this](){ this->handleWiFi(); });
    server->on("/connect", HTTP_POST, [this](){ this->handleConnect(); });
    server->on("/guest", [this](){ this->handleGuest(); });
    server->on("/user", [this](){ this->handleUserMode(); });
    server->on("/login", [this](){ this->handleUserLogin(); });
    server->on("/login-attempt", HTTP_POST, [this](){ this->handleLoginAttempt(); });
    server->onNotFound([this](){ this->handleNotFound(); });
    server->begin();
    Serial.println("HTTP server started");
}

void WiFiManager::setupAPMode() {
    Serial.println("Setting up AP Mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ap_ssid, ap_password);
    
    if (setupUICallback) {
        setupUICallback();
    }
    
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    
    // Setup captive portal
    dnsServer->start(53, "*", apIP);
    
    // Start MDNS responder
    if (MDNS.begin("healthsense")) {
        Serial.println("MDNS responder started");
    }
    
    apModeActive = true;
}

bool WiFiManager::connectToWiFi(String ssid, String password) {
    if (ssid.length() == 0) {
        return false;
    }
    
    Serial.println("Connecting to WiFi");
    Serial.print("SSID: ");
    Serial.println(ssid);
    
    WiFi.begin(ssid.c_str(), password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        
        if (updateConnectionStatusCallback) {
            updateConnectionStatusCallback(true, false);
        }
        
        isConnected = true;
        return true;
    } else {
        Serial.println("");
        Serial.println("WiFi connection failed");
        
        if (updateConnectionStatusCallback) {
            updateConnectionStatusCallback(false, false);
        }
        
        return false;
    }
}

void WiFiManager::readWiFiCredentials() {
    EEPROM.begin(EEPROM_SIZE);
    
    // Read SSID
    char ssid[64];
    for (int i = 0; i < 64; i++) {
        ssid[i] = EEPROM.read(SSID_ADDR + i);
    }
    userSSID = String(ssid);
    
    // Read password
    char password[64];
    for (int i = 0; i < 64; i++) {
        password[i] = EEPROM.read(PASS_ADDR + i);
    }
    userPassword = String(password);
    
    // Read mode
    isGuestMode = (EEPROM.read(MODE_ADDR) == 1);
    
    EEPROM.end();
}

void WiFiManager::saveWiFiCredentials(String ssid, String password, bool guestMode) {
    EEPROM.begin(EEPROM_SIZE);
    
    // Save SSID
    for (int i = 0; i < 64; i++) {
        if (i < ssid.length()) {
            EEPROM.write(SSID_ADDR + i, ssid[i]);
        } else {
            EEPROM.write(SSID_ADDR + i, 0);
        }
    }
    
    // Save password
    for (int i = 0; i < 64; i++) {
        if (i < password.length()) {
            EEPROM.write(PASS_ADDR + i, password[i]);
        } else {
            EEPROM.write(PASS_ADDR + i, 0);
        }
    }
    
    // Save mode
    EEPROM.write(MODE_ADDR, guestMode ? 1 : 0);
    
    EEPROM.commit();
    EEPROM.end();
}

void WiFiManager::checkWiFiConnection() {
    // Only check periodically
    if ((millis() - lastWifiCheck < wifiCheckInterval) && isConnected) {
        return;
    }
    
    lastWifiCheck = millis();
    
    // If we were connected but lost connection
    if (isConnected && WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost!");
        isConnected = false;
        
        // Start AP mode
        setupAPMode();
        
        if (updateConnectionStatusCallback) {
            updateConnectionStatusCallback(false, isGuestMode);
        }
    }
}

void WiFiManager::handleRoot() {
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>HealthSense WiFi Setup</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                  ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                  "h1 { color: #333; }"
                  "button, input[type='submit'] { background: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 0; width: 100%; }"
                  "button:hover, input[type='submit']:hover { background: #45a049; }"
                  "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }"
                  ".guest-btn { background: #2196F3; }"
                  ".guest-btn:hover { background: #0b7dda; }"
                  ".user-btn { background: #4CAF50; }"
                  ".user-btn:hover { background: #45a049; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>HealthSense Setup</h1>"
                  "<p>Choose your connection option:</p>"
                  "<form action='/user' method='get'><button type='submit' class='user-btn'>User Mode</button></form>"
                  "<form action='/guest' method='get'><button type='submit' class='guest-btn'>Guest Mode</button></form>"
                  "</div>"
                  "</body></html>";
    server->send(200, "text/html", html);
}

void WiFiManager::handleWiFi() {
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>HealthSense WiFi Setup</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                  ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                  "h1 { color: #333; }"
                  "button, input[type='submit'] { background: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 0; width: 100%; }"
                  "button:hover, input[type='submit']:hover { background: #45a049; }"
                  "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }"
                  ".back-btn { background: #f44336; }"
                  ".back-btn:hover { background: #d32f2f; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>Connect to WiFi</h1>"
                  "<form action='/connect' method='post'>"
                  "<label for='ssid'>WiFi Network Name:</label><br>"
                  "<input type='text' id='ssid' name='ssid' required><br>"
                  "<label for='password'>WiFi Password:</label><br>"
                  "<input type='password' id='password' name='password'><br>"
                  "<input type='submit' value='Connect'>"
                  "</form>"
                  "<form action='/' method='get'><button type='submit' class='back-btn'>Back</button></form>"
                  "</div>"
                  "</body></html>";
    server->send(200, "text/html", html);
}

void WiFiManager::handleConnect() {
    String ssid = server->arg("ssid");
    String password = server->arg("password");
    
    if (ssid.length() > 0) {
        // Save credentials
        userSSID = ssid;
        userPassword = password;
        isGuestMode = false;
        saveWiFiCredentials(ssid, password, false);
        
        String html = "<!DOCTYPE html><html>"
                    "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                    "<title>HealthSense WiFi Setup</title>"
                    "<style>"
                    "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                    ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                    "h1 { color: #333; }"
                    ".success { color: #4CAF50; }"
                    "</style>"
                    "</head>"
                    "<body>"
                    "<div class='container'>"
                    "<h1>Connection Status</h1>"
                    "<p class='success'>WiFi credentials saved!</p>"
                    "<p>The device will now attempt to connect to your WiFi network.</p>"
                    "<p><strong>USER LOGGED IN</strong></p>"
                    "</div>"
                    "</body></html>";
        server->send(200, "text/html", html);
        
        // Try to connect to the WiFi
        isConnected = connectToWiFi(ssid, password);
        
        if (isConnected) {
            // Set WiFi mode to both AP and STA - so hotspot remains available
            WiFi.mode(WIFI_AP_STA);
            
            // Initialize sensor if connected and callback exists
            if (initializeSensorCallback) {
                initializeSensorCallback();
            }
            
            if (updateConnectionStatusCallback) {
                updateConnectionStatusCallback(true, false);
            }
        } else {
            apModeActive = true;
            
            if (updateConnectionStatusCallback) {
                updateConnectionStatusCallback(false, false);
            }
        }
    } else {
        // Redirect to WiFi setup page
        server->sendHeader("Location", "/wifi");
        server->send(302, "text/plain", "");
    }
}

void WiFiManager::handleGuest() {
    isGuestMode = true;
    isAuthenticated = false;
    saveWiFiCredentials("", "", true);
    
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>HealthSense WiFi Setup</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                  ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                  "h1 { color: #333; }"
                  ".success { color: #2196F3; }"
                  "a { color: #4CAF50; text-decoration: none; font-weight: bold; }"
                  "a:hover { text-decoration: underline; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>Guest Mode Activated</h1>"
                  "<p class='success'>The device will operate in Guest Mode.</p>"
                  "<p>You can measure your heart rate and SpO2 but data won't be saved to your account.</p>"
                  "<p>Create an account at <a href='http://localhost:30000' target='_blank'>localhost:30000</a> to save your measurements.</p>"
                  "<p><strong>GUEST MODE</strong></p>"
                  "</div>"
                  "</body></html>";
    server->send(200, "text/html", html);
    
    // Initialize sensor if callback exists
    if (initializeSensorCallback) {
        initializeSensorCallback();
    }
    
    if (updateConnectionStatusCallback) {
        updateConnectionStatusCallback(false, true); // Not connected to WiFi, but in guest mode
    }
    
    // We don't restart the device, we'll keep AP mode active
    apModeActive = true;
}

void WiFiManager::handleUserMode() {
    // Check if WiFi is already connected
    if (isConnected) {
        // If connected, show login page
        handleUserLogin();
    } else {
        // If not connected, show WiFi setup page
        handleWiFi();
    }
}

void WiFiManager::handleUserLogin() {
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>HealthSense User Login</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                  ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                  "h1 { color: #333; }"
                  "button, input[type='submit'] { background: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 0; width: 100%; }"
                  "button:hover, input[type='submit']:hover { background: #45a049; }"
                  "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }"
                  ".back-btn { background: #f44336; }"
                  ".back-btn:hover { background: #d32f2f; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>User Login</h1>"
                  "<form action='/login-attempt' method='post'>"
                  "<label for='username'>Username:</label><br>"
                  "<input type='text' id='username' name='username' required><br>"
                  "<label for='password'>Password:</label><br>"
                  "<input type='password' id='password' name='password' required><br>"
                  "<input type='submit' value='Login'>"
                  "</form>"
                  "<form action='/' method='get'><button type='submit' class='back-btn'>Back</button></form>"
                  "</div>"
                  "</body></html>";
    server->send(200, "text/html", html);
}

void WiFiManager::handleLoginAttempt() {
    String username = server->arg("username");
    String password = server->arg("password");
    
    if (username.length() > 0 && password.length() > 0) {
        // Save credentials locally
        this->username = username;
        this->password = password;
        isGuestMode = false;
        
        // Attempt to authenticate
        bool authSuccess = authenticateUser(username, password);
        String uid = authSuccess ? apiService.getUserId() : "";
        
        // Generate response HTML
        String html = "<!DOCTYPE html><html>"
                    "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                    "<title>HealthSense Authentication</title>"
                    "<style>"
                    "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                    ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                    "h1 { color: #333; }"
                    ".success { color: #4CAF50; }"
                    ".error { color: #f44336; }"
                    "</style>"
                    "</head>"
                    "<body>"
                    "<div class='container'>"
                    "<h1>Authentication Status</h1>";
        
        if (authSuccess) {
            html += "<p class='success'>Authentication successful!</p>"
                   "<p>Your User ID: <strong>" + uid + "</strong></p>"
                   "<p>Measurements will now be sent to your account.</p>";
        } else {
            html += "<p class='error'>Authentication failed!</p>"
                   "<p>Please check your username and password.</p>"
                   "<form action='/user' method='get'><button type='submit'>Try Again</button></form>";
        }
        
        html += "</div></body></html>";
        server->send(200, "text/html", html);
        
        // Call authentication callback if exists
        if (authenticationStatusCallback) {
            authenticationStatusCallback(authSuccess, uid);
        }
        
        // If authenticated, initialize sensor
        if (authSuccess && initializeSensorCallback) {
            initializeSensorCallback();
        }
        
        // Update connection status
        if (updateConnectionStatusCallback) {
            updateConnectionStatusCallback(isConnected, false); // Not in guest mode
        }
    } else {
        // Invalid input, redirect to login page
        server->sendHeader("Location", "/user");
        server->send(302, "text/plain", "");
    }
}

void WiFiManager::handleNotFound() {
    server->sendHeader("Location", "http://" + apIP.toString(), true);
    server->send(302, "text/plain", "");
}

bool WiFiManager::authenticateUser(String username, String password) {
    // Attempt to authenticate via API service
    bool success = apiService.authenticateUser(username, password);
    isAuthenticated = success;
    return success;
}

bool WiFiManager::sendHealthData(int heartRate, int spo2) {
    // Only send data if authenticated
    if (!isAuthenticated) {
        return false;
    }
    
    bool success = apiService.sendHealthData(heartRate, spo2);
    
    // Call data transmission callback if exists
    if (dataTransmissionCallback) {
        dataTransmissionCallback(success);
    }
    
    return success;
}

void WiFiManager::loop() {
    // Process DNS and server requests
    dnsServer->processNextRequest();
    server->handleClient();
    
    // Check WiFi connection status
    checkWiFiConnection();
}

void WiFiManager::setSetupUICallback(void (*callback)()) {
    setupUICallback = callback;
}

void WiFiManager::setInitializeSensorCallback(void (*callback)()) {
    initializeSensorCallback = callback;
}

void WiFiManager::setUpdateConnectionStatusCallback(void (*callback)(bool connected, bool guestMode)) {
    updateConnectionStatusCallback = callback;
}

void WiFiManager::setAuthenticationStatusCallback(void (*callback)(bool authenticated, String uid)) {
    authenticationStatusCallback = callback;
}

void WiFiManager::setDataTransmissionCallback(void (*callback)(bool success)) {
    dataTransmissionCallback = callback;
}
