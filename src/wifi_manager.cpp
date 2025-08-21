#include "wifi_manager.h"
#include <EEPROM.h>

// EEPROM constants
#define EEPROM_SIZE 1024
#define SSID_ADDR 0
#define PASS_ADDR 64
#define MODE_ADDR 128
#define EMAIL_ADDR 192
#define UID_ADDR 256

WiFiManager::WiFiManager(const char* ap_ssid, const char* ap_password, const char* serverURL) :
    ap_ssid(ap_ssid),
    ap_password(ap_password),
    serverURL(serverURL),
    isConnected(false),
    isGuestMode(false),
    isLoggedIn(false),
    apModeActive(false),
    isMeasuring(false),
    lastWifiCheck(0),
    wifiCheckInterval(5000),
    setupUICallback(nullptr),
    initializeSensorCallback(nullptr),
    updateConnectionStatusCallback(nullptr),
    sendDataCallback(nullptr)
{
    apIP = IPAddress(192, 168, 4, 1);
    server = new WebServer(80);
    dnsServer = new DNSServer();
}

void WiFiManager::begin() {
    // Read saved WiFi credentials
    readWiFiCredentials();
    
    // Always start in AP mode first
    setupAPMode();
    
    // Try to connect to saved WiFi if credentials exist
    if (userSSID.length() > 0) {
        Serial.println("Attempting to connect to saved WiFi...");
        isConnected = connectToWiFi(userSSID, userPassword);
        
        if (isConnected) {
            Serial.println("Auto-connected to saved WiFi network");
        } else {
            Serial.println("Failed to auto-connect to saved WiFi");
        }
    }
    
    // Setup web server
    server->on("/", [this](){ this->handleRoot(); });
    server->on("/wifi", [this](){ this->handleWiFi(); });
    server->on("/connect", HTTP_POST, [this](){ this->handleConnect(); });
    server->on("/mode", [this](){ this->handleModeSelect(); });
    server->on("/login", [this](){ this->handleLogin(); });
    server->on("/login_submit", HTTP_POST, [this](){ this->handleLoginSubmit(); });
    server->on("/guest", [this](){ this->handleGuest(); });
    server->on("/measurement", [this](){ this->handleMeasurement(); });
    server->on("/reconfigure_wifi", [this](){ this->handleReconfigWiFi(); });
    server->on("/status", [this](){ this->handleStatus(); });
    server->on("/force_ap", [this](){ this->handleForceAP(); });
    server->onNotFound([this](){ this->handleNotFound(); });
    server->begin();
    Serial.println("HTTP server started");
    
    // Print connection information
    Serial.println("\n" + getConnectionInfo());
}

void WiFiManager::setupAPMode() {
    Serial.println("Setting up AP Mode");
    
    // If WiFi is already connected, use dual mode (AP + STA)
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.mode(WIFI_AP_STA);
        Serial.println("Using dual mode (AP + Station)");
    } else {
        WiFi.mode(WIFI_AP);
        Serial.println("Using AP mode only");
    }
    
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
    
    // Use dual mode to maintain AP while connecting to WiFi
    WiFi.mode(WIFI_AP_STA);
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
        Serial.print("AP IP address still available: ");
        Serial.println(WiFi.softAPIP());
        
        if (updateConnectionStatusCallback) {
            updateConnectionStatusCallback(true, false, isLoggedIn);
        }
        
        isConnected = true;
        return true;
    } else {
        Serial.println("");
        Serial.println("WiFi connection failed");
        
        // If connection failed, ensure AP mode is still active
        if (!apModeActive) {
            setupAPMode();
        }
        
        if (updateConnectionStatusCallback) {
            updateConnectionStatusCallback(false, false, isLoggedIn);
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
    
    // Read email
    char email[64];
    for (int i = 0; i < 64; i++) {
        email[i] = EEPROM.read(EMAIL_ADDR + i);
    }
    userEmail = String(email);
    
    // Read UID
    char uid[64];
    for (int i = 0; i < 64; i++) {
        uid[i] = EEPROM.read(UID_ADDR + i);
    }
    userUID = String(uid);
    
    // Set login status based on UID
    isLoggedIn = (userUID.length() > 0 && !isGuestMode);
    
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
    
    // If guest mode, clear email and UID
    if (guestMode) {
        for (int i = 0; i < 64; i++) {
            EEPROM.write(EMAIL_ADDR + i, 0);
            EEPROM.write(UID_ADDR + i, 0);
        }
        userEmail = "";
        userUID = "";
        isLoggedIn = false;
    }
    
    EEPROM.commit();
    EEPROM.end();
}

void WiFiManager::checkWiFiConnection() {
    // Only check periodically
    if ((millis() - lastWifiCheck < wifiCheckInterval)) {
        return;
    }
    
    lastWifiCheck = millis();
    
    // Check if we lost WiFi connection
    if (isConnected && WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost!");
        isConnected = false;
        
        // Try to reconnect with saved credentials
        if (userSSID.length() > 0) {
            Serial.println("Attempting to reconnect...");
            WiFi.mode(WIFI_AP_STA);
            WiFi.begin(userSSID.c_str(), userPassword.c_str());
            
            // Give it a few seconds to reconnect
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 10) {
                delay(500);
                Serial.print(".");
                attempts++;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("\nReconnected to WiFi!");
                isConnected = true;
                
                if (updateConnectionStatusCallback) {
                    updateConnectionStatusCallback(true, isGuestMode, isLoggedIn);
                }
                return;
            } else {
                Serial.println("\nFailed to reconnect");
            }
        }
        
        // Ensure AP mode is still active for reconfiguration
        if (!apModeActive) {
            setupAPMode();
        }
        
        if (updateConnectionStatusCallback) {
            updateConnectionStatusCallback(false, isGuestMode, isLoggedIn);
        }
    }
    
    // Check if we need to restart AP mode (in case it was disabled)
    if (!apModeActive && WiFi.getMode() != WIFI_AP_STA && WiFi.getMode() != WIFI_AP) {
        Serial.println("AP mode not active, restarting...");
        setupAPMode();
    }
}

void WiFiManager::handleRoot() {
    // If already connected to WiFi, go to mode selection
    if (isConnected) {
        server->sendHeader("Location", "/mode");
        server->send(302, "text/plain", "");
        return;
    }
    
    // Otherwise show WiFi setup
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>HealthSense WiFi Setup</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                  ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                  "h1 { color: #333; }"
                  ".status { font-weight: bold; margin-bottom: 20px; }"
                  ".connected { color: #4CAF50; }"
                  ".disconnected { color: #f44336; }"
                  "button, input[type='submit'] { background: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 0; width: 100%; }"
                  "button:hover, input[type='submit']:hover { background: #45a049; }"
                  "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }"
                  ".guest-btn { background: #2196F3; }"
                  ".guest-btn:hover { background: #0b7dda; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>HealthSense Setup</h1>";
    
    // Show WiFi status
    if (isConnected) {
        html += "<p class='status connected'>WiFi Connected to: " + userSSID + "</p>";
        html += "<p class='status'>Station IP: " + WiFi.localIP().toString() + "</p>";
    } else {
        html += "<p class='status disconnected'>WiFi Not Connected</p>";
    }
    
    html += "<p class='status'>Hotspot IP: " + WiFi.softAPIP().toString() + "</p>";
    html += "<p style='font-size: 12px; color: #666;'>Access this device from both WiFi network and hotspot</p>";
    
    html += "<p>Configure your WiFi connection:</p>"
            "<form action='/wifi' method='get'><button type='submit'>Setup WiFi</button></form>";
    
    if (isConnected) {
        html += "<form action='/mode' method='get'><button type='submit'>Continue to Mode Selection</button></form>";
    }
    
    html += "<form action='/status' method='get'><button type='submit' class='guest-btn'>Connection Status</button></form>";
    
    html += "</div></body></html>";
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
                    ".error { color: #f44336; }"
                    "</style>"
                    "</head>"
                    "<body>"
                    "<div class='container'>"
                    "<h1>Connection Status</h1>";
        
        // Try to connect to the WiFi
        isConnected = connectToWiFi(ssid, password);
        
        if (isConnected) {
            html += "<p class='success'>WiFi connected successfully!</p>"
                    "<p>Connected to: " + ssid + "</p>"
                    "<p>IP Address: " + WiFi.localIP().toString() + "</p>"
                    "<p>You can also access this device at: " + WiFi.softAPIP().toString() + " (Hotspot)</p>"
                    "<meta http-equiv='refresh' content='3;url=/mode'>"
                    "<p>You will be redirected to mode selection in 3 seconds...</p>";
            
            // Maintain dual mode (AP + STA) so hotspot remains available
            // This is already set in connectToWiFi function
        } else {
            html += "<p class='error'>Failed to connect to WiFi!</p>"
                    "<p>Please check your credentials and try again.</p>"
                    "<meta http-equiv='refresh' content='3;url=/wifi'>"
                    "<p>You will be redirected to WiFi setup in 3 seconds...</p>";
            
            // Ensure AP mode is active for reconfiguration
            if (!apModeActive) {
                setupAPMode();
            }
        }
        
        html += "</div></body></html>";
        server->send(200, "text/html", html);
        
        // Update connection status
        if (updateConnectionStatusCallback) {
            updateConnectionStatusCallback(isConnected, false, false);
        }
    } else {
        // Redirect to WiFi setup page
        server->sendHeader("Location", "/wifi");
        server->send(302, "text/plain", "");
    }
}

void WiFiManager::handleModeSelect() {
    // If not connected to WiFi, redirect to WiFi setup
    if (!isConnected) {
        server->sendHeader("Location", "/");
        server->send(302, "text/plain", "");
        return;
    }
    
    // Reset measurement state
    isMeasuring = false;
    
    String html = "<!DOCTYPE html><html>"
                "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<title>HealthSense Mode Selection</title>"
                "<style>"
                "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                "h1 { color: #333; }"
                "button, input[type='submit'] { background: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 0; width: 100%; }"
                "button:hover, input[type='submit']:hover { background: #45a049; }"
                ".guest-btn { background: #2196F3; }"
                ".guest-btn:hover { background: #0b7dda; }"
                ".reconfigure-btn { background: #f44336; margin-top: 30px; }"
                ".reconfigure-btn:hover { background: #d32f2f; }"
                "</style>"
                "</head>"
                "<body>"
                "<div class='container'>"
                "<h1>HealthSense Mode Selection</h1>"
                "<p>Choose your operating mode:</p>"
                "<form action='/login' method='get'><button type='submit'>User Mode</button></form>"
                "<form action='/guest' method='get'><button type='submit' class='guest-btn'>Guest Mode</button></form>"
                "<form action='/reconfigure_wifi' method='get'><button type='submit' class='reconfigure-btn'>Reconfigure WiFi</button></form>"
                "</div>"
                "</body></html>";
    
    server->send(200, "text/html", html);
}

void WiFiManager::handleLogin() {
    // If not connected to WiFi, redirect to WiFi setup
    if (!isConnected) {
        server->sendHeader("Location", "/");
        server->send(302, "text/plain", "");
        return;
    }
    
    String html = "<!DOCTYPE html><html>"
                "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<title>HealthSense Login</title>"
                "<style>"
                "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                "h1 { color: #333; }"
                "button, input[type='submit'] { background: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 0; width: 100%; }"
                "button:hover, input[type='submit']:hover { background: #45a049; }"
                "input[type='text'], input[type='password'], input[type='email'] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }"
                ".back-btn { background: #f44336; }"
                ".back-btn:hover { background: #d32f2f; }"
                "</style>"
                "</head>"
                "<body>"
                "<div class='container'>"
                "<h1>User Login</h1>"
                "<form action='/login_submit' method='post'>"
                "<label for='email'>Email:</label><br>"
                "<input type='email' id='email' name='email' required><br>"
                "<label for='password'>Password:</label><br>"
                "<input type='password' id='password' name='password' required><br>"
                "<input type='submit' value='Login'>"
                "</form>"
                "<form action='/mode' method='get'><button type='submit' class='back-btn'>Back</button></form>"
                "</div>"
                "</body></html>";
    
    server->send(200, "text/html", html);
}

void WiFiManager::handleLoginSubmit() {
    String email = server->arg("email");
    String password = server->arg("password");
    
    String html = "<!DOCTYPE html><html>"
                "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<title>HealthSense Login</title>"
                "<style>"
                "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                "h1 { color: #333; }"
                ".success { color: #4CAF50; }"
                ".error { color: #f44336; }"
                "button { background: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 0; width: 100%; }"
                "button:hover { background: #45a049; }"
                ".back-btn { background: #2196F3; margin-top: 15px; }"
                ".back-btn:hover { background: #0b7dda; }"
                "</style>"
                "</head>"
                "<body>"
                "<div class='container'>"
                "<h1>Login Status</h1>";
    
    Serial.println("Attempting to authenticate user");
    Serial.print("Email: ");
    Serial.println(email);
    
    // Authenticate user with API
    bool loginSuccess = authenticateUser(email, password);
    
    if (loginSuccess) {
        html += "<p class='success'>Login successful!</p>"
                "<p>Welcome back, " + email + "!</p>"
                "<meta http-equiv='refresh' content='2;url=/measurement'>"
                "<p>You will be redirected to measurement in 2 seconds...</p>";
        
        Serial.println("Login successful, user authenticated");
        
        // Update connection status and set user as logged in
        if (updateConnectionStatusCallback) {
            updateConnectionStatusCallback(isConnected, false, true);
        }
        
        // Initialize sensor if callback exists
        if (initializeSensorCallback) {
            initializeSensorCallback();
        }
        
        isMeasuring = true;
    } else {
        html += "<p class='error'>Login failed!</p>"
                "<p>Invalid email or password. Please try again.</p>"
                "<form action='/login' method='get'>"
                "<button type='submit' class='back-btn'>Back to Login</button>"
                "</form>"
                "<form action='/mode' method='get'>"
                "<button type='submit'>Back to Mode Selection</button>"
                "</form>";
                
        Serial.println("Login failed, invalid credentials");
    }
    
    html += "</div></body></html>";
    server->send(200, "text/html", html);
}

void WiFiManager::handleGuest() {
    isGuestMode = true;
    isLoggedIn = false;
    saveWiFiCredentials("", "", true);
    
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>HealthSense Guest Mode</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                  ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                  "h1 { color: #333; }"
                  ".success { color: #2196F3; }"
                  "a { color: #2196F3; text-decoration: none; font-weight: bold; }"
                  "a:hover { text-decoration: underline; }"
                  ".reconfigure-btn { background: #f44336; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin-top: 30px; width: 100%; }"
                  ".reconfigure-btn:hover { background: #d32f2f; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>Guest Mode Activated</h1>"
                  "<p class='success'>The device is now operating in Guest Mode.</p>"
                  "<p>You can measure your vital signs without creating an account.</p>"
                  "<p>To save your measurements and track your health over time, please register at: "
                  "<a href='http://localhost:30001' target='_blank'>HealthSense Portal</a></p>"
                  "<meta http-equiv='refresh' content='2;url=/measurement'>"
                  "<p>You will be redirected to measurement in 2 seconds...</p>"
                  "</div>"
                  "</body></html>";
    server->send(200, "text/html", html);
    
    // Initialize sensor if callback exists
    if (initializeSensorCallback) {
        initializeSensorCallback();
    }
    
    if (updateConnectionStatusCallback) {
        updateConnectionStatusCallback(isConnected, true, false);
    }
    
    isMeasuring = true;
}

void WiFiManager::handleMeasurement() {
    // If not in guest mode and not logged in, redirect to mode selection
    if (!isGuestMode && !isLoggedIn) {
        server->sendHeader("Location", "/mode");
        server->send(302, "text/plain", "");
        return;
    }
    
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>HealthSense Measurement</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                  ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                  "h1 { color: #333; }"
                  ".reading { font-size: 24px; margin: 20px 0; }"
                  ".hr { color: #f44336; }"
                  ".spo2 { color: #2196F3; }"
                  ".status { font-style: italic; color: #757575; margin-bottom: 20px; }"
                  ".user-status { font-weight: bold; color: #4CAF50; margin: 10px 0; }"
                  ".guest-status { font-weight: bold; color: #FF9800; margin: 10px 0; }"
                  "a { color: #2196F3; text-decoration: none; font-weight: bold; }"
                  "a:hover { text-decoration: underline; }"
                  ".reconfigure-btn { background: #f44336; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin-top: 30px; width: 100%; }"
                  ".reconfigure-btn:hover { background: #d32f2f; }"
                  "</style>"
                  "<script>"
                  "function refreshReadings() {"
                  "  // In a real implementation, this would fetch from an endpoint"
                  "  // For this demo, we'll just reload the page every 10 seconds"
                  "  setTimeout(function() { location.reload(); }, 10000);"
                  "}"
                  "window.onload = function() { refreshReadings(); }"
                  "</script>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>HealthSense Monitor</h1>";
    
    if (isLoggedIn) {
        html += "<p class='user-status'>User Mode - Data is being saved</p>";
    } else {
        html += "<p class='guest-status'>Guest Mode - Data is not being saved</p>"
                "<p>Register at: <a href='https://iot.newnol.io.vn' target='_blank'>HealthSense Portal</a></p>";
    }
    
    html += "<p class='status'>Place your finger on the sensor to measure</p>"
            "<div class='reading hr'>Heart Rate: <span id='hr'>--</span> BPM</div>"
            "<div class='reading spo2'>SpO2: <span id='spo2'>--</span> %</div>";
    
    // Show different buttons based on whether this is guest mode or user mode
    if (isGuestMode) {
        html += "<form action='/mode' method='get'>"
               "<button type='submit' class='reconfigure-btn'>Back to Mode Selection</button>"
               "</form>";
    } else {
        // In user mode, we only show the "Back to Mode Selection" button
        html += "<form action='/mode' method='get'>"
               "<button type='submit' class='reconfigure-btn'>Back to Mode Selection</button>"
               "</form>";
    }
    
    html += "</div>"
            "</body></html>";
    
    server->send(200, "text/html", html);
    
    // Make sure we're measuring
    isMeasuring = true;
}

void WiFiManager::handleReconfigWiFi() {
    // Stop measuring when reconfiguring WiFi
    isMeasuring = false;
    
    // Return to WiFi setup page - explicitly go to WiFi setup page
    server->sendHeader("Location", "/wifi");
    server->send(302, "text/plain", "");
    
    // Update connection status
    if (updateConnectionStatusCallback) {
        updateConnectionStatusCallback(isConnected, false, false);
    }
}

void WiFiManager::handleStatus() {
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>HealthSense Connection Status</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                  ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                  "h1 { color: #333; }"
                  ".status-info { text-align: left; background: #f9f9f9; padding: 15px; border-radius: 5px; margin: 15px 0; font-family: monospace; }"
                  "button { background: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 5px; }"
                  "button:hover { background: #45a049; }"
                  ".back-btn { background: #2196F3; }"
                  ".back-btn:hover { background: #0b7dda; }"
                  ".force-btn { background: #f44336; }"
                  ".force-btn:hover { background: #d32f2f; }"
                  ".refresh-btn { background: #FF9800; }"
                  ".refresh-btn:hover { background: #e68900; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>Connection Status</h1>"
                  "<div class='status-info'>";
    
    // Add detailed connection information
    html += getConnectionInfo();
    
    html += "</div>"
            "<p><strong>How to Connect:</strong></p>"
            "<ul style='text-align: left; margin: 20px 0;'>";
    
    if (isConnected) {
        html += "<li>Via WiFi Network: Connect to '" + userSSID + "' and access " + WiFi.localIP().toString() + "</li>";
    }
    
    html += "<li>Via Hotspot: Connect to '" + String(ap_ssid) + "' and access " + WiFi.softAPIP().toString() + "</li>"
            "</ul>"
            "<div style='margin-top: 30px;'>"
            "<form action='/' method='get' style='display: inline;'><button type='submit' class='back-btn'>Back to Home</button></form>"
            "<button onclick='location.reload()' class='refresh-btn'>Refresh Status</button>";
    
    if (isConnected) {
        html += "<form action='/force_ap' method='get' style='display: inline;'><button type='submit' class='force-btn'>Force Hotspot Only</button></form>";
    }
    
    html += "</div>"
            "</div>"
            "</body></html>";
    
    server->send(200, "text/html", html);
}

void WiFiManager::handleForceAP() {
    forceAPMode();
    
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>HealthSense Force AP Mode</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                  ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                  "h1 { color: #333; }"
                  ".success { color: #4CAF50; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>AP Mode Forced</h1>"
                  "<p class='success'>Device is now in Access Point mode only.</p>"
                  "<p>WiFi connection has been disconnected.</p>"
                  "<meta http-equiv='refresh' content='3;url=/'>"
                  "<p>You will be redirected to home in 3 seconds...</p>"
                  "</div>"
                  "</body></html>";
    
    server->send(200, "text/html", html);
}

void WiFiManager::handleNotFound() {
    server->sendHeader("Location", "http://" + apIP.toString(), true);
    server->send(302, "text/plain", "");
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

void WiFiManager::setUpdateConnectionStatusCallback(void (*callback)(bool connected, bool guestMode, bool loggedIn)) {
    updateConnectionStatusCallback = callback;
}

void WiFiManager::setSendDataCallback(void (*callback)(String uid, int32_t heartRate, int32_t spo2)) {
    sendDataCallback = callback;
}

void WiFiManager::saveUserCredentials(String email, String uid) {
    EEPROM.begin(EEPROM_SIZE);
    
    // Save email
    for (int i = 0; i < 64; i++) {
        if (i < email.length()) {
            EEPROM.write(EMAIL_ADDR + i, email[i]);
        } else {
            EEPROM.write(EMAIL_ADDR + i, 0);
        }
    }
    
    // Save UID
    for (int i = 0; i < 64; i++) {
        if (i < uid.length()) {
            EEPROM.write(UID_ADDR + i, uid[i]);
        } else {
            EEPROM.write(UID_ADDR + i, 0);
        }
    }
    
    // Update member variables
    userEmail = email;
    userUID = uid;
    isLoggedIn = (uid.length() > 0);
    isGuestMode = false;
    
    // Save mode
    EEPROM.write(MODE_ADDR, 0); // Not guest mode
    
    EEPROM.commit();
    EEPROM.end();
}

bool WiFiManager::authenticateUser(String email, String password) {
    if (!isConnected) return false;
    
    HTTPClient http;
    String url = "https://iot.newnol.io.vn/api/login";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    // Create JSON payload
    DynamicJsonDocument doc(200);
    doc["email"] = email;
    doc["password"] = password;
    String payload;
    serializeJson(doc, payload);
    
    // Send POST request
    int httpCode = http.POST(payload);
    Serial.print("Login API response code: ");
    Serial.println(httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
        // Parse response
        String response = http.getString();
        Serial.print("Login API response: ");
        Serial.println(response);
        
        DynamicJsonDocument responseDoc(400);
        DeserializationError error = deserializeJson(responseDoc, response);
        
        if (!error) {
            // Successful login should have a uid field
            const char* uid = responseDoc["uid"];
            if (uid && strlen(uid) > 0) {
                // Save user credentials
                saveUserCredentials(email, String(uid));
                isLoggedIn = true;
                http.end();
                return true;
            } else {
                Serial.println("Login failed: No valid UID in response");
            }
        } else {
            Serial.print("JSON parse error: ");
            Serial.println(error.c_str());
        }
    } else {
        // Handle different error cases
        String response = http.getString();
        Serial.print("Login failed with response: ");
        Serial.println(response);
        
        // Try to parse error message
        DynamicJsonDocument errorDoc(400);
        DeserializationError error = deserializeJson(errorDoc, response);
        
        if (!error) {
            // Check for detail field which contains error information
            const char* errorDetail = errorDoc["detail"];
            if (errorDetail) {
                Serial.print("Error detail: ");
                Serial.println(errorDetail);
                
                // Handle specific error cases
                if (strcmp(errorDetail, "INVALID_LOGIN_CREDENTIALS") == 0) {
                    Serial.println("Invalid email or password");
                } else if (strcmp(errorDetail, "Authentication service unavailable") == 0) {
                    Serial.println("Firebase service is unavailable");
                } else if (strcmp(errorDetail, "Missing Firebase API key") == 0) {
                    Serial.println("Server configuration error: Missing Firebase API key");
                }
            }
        }
    }
    
    http.end();
    return false;
}

bool WiFiManager::sendMeasurementData(String uid, int32_t heartRate, int32_t spo2) {
    if (!isConnected && !isGuestMode) return false;
    
    // In guest mode, we don't send data to server
    if (isGuestMode) return true;
    
    HTTPClient http;
    String url = serverURL + "/api/measurements";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    // Create JSON payload
    DynamicJsonDocument doc(200);
    doc["uid"] = uid;
    doc["heartRate"] = heartRate;
    doc["spo2"] = spo2;
    String payload;
    serializeJson(doc, payload);
    
    // Send POST request
    int httpCode = http.POST(payload);
    Serial.print("Measurement API response code: ");
    Serial.println(httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
        Serial.println("Measurement data sent successfully");
    } else {
        Serial.print("Failed to send measurement data: ");
        Serial.println(http.errorToString(httpCode));
    }
    
    http.end();
    return (httpCode == HTTP_CODE_OK);
}

void WiFiManager::sendSensorData(int32_t heartRate, int32_t spo2) {
    if (isMeasuring) {
        // If in guest mode, no need to send to server
        if (isGuestMode) {
            // Just call the callback if it exists
            if (sendDataCallback) {
                sendDataCallback("guest", heartRate, spo2);
            }
            return;
        }
        
        // If logged in, send to server
        if (isLoggedIn && userUID.length() > 0) {
            sendMeasurementData(userUID, heartRate, spo2);
            
            // Call callback if it exists
            if (sendDataCallback) {
                sendDataCallback(userUID, heartRate, spo2);
            }
        }
    }
}

String WiFiManager::getConnectionInfo() const {
    String info = "Connection Status:\n";
    info += "- WiFi Mode: ";
    
    switch (WiFi.getMode()) {
        case WIFI_AP:
            info += "Access Point Only\n";
            break;
        case WIFI_STA:
            info += "Station Only\n";
            break;
        case WIFI_AP_STA:
            info += "Dual Mode (AP + Station)\n";
            break;
        default:
            info += "Off\n";
            break;
    }
    
    if (isConnected) {
        info += "- Connected to: " + userSSID + "\n";
        info += "- Station IP: " + WiFi.localIP().toString() + "\n";
    } else {
        info += "- Not connected to WiFi\n";
    }
    
    if (apModeActive) {
        info += "- Hotspot Active: " + String(ap_ssid) + "\n";
        info += "- Hotspot IP: " + WiFi.softAPIP().toString() + "\n";
    } else {
        info += "- Hotspot: Inactive\n";
    }
    
    return info;
}

void WiFiManager::forceAPMode() {
    Serial.println("Forcing AP mode...");
    WiFi.disconnect();
    isConnected = false;
    setupAPMode();
    
    if (updateConnectionStatusCallback) {
        updateConnectionStatusCallback(false, isGuestMode, isLoggedIn);
    }
}

void WiFiManager::restartWiFi() {
    Serial.println("Restarting WiFi...");
    WiFi.disconnect();
    delay(1000);
    
    isConnected = false;
    
    // Try to reconnect with saved credentials
    if (userSSID.length() > 0) {
        isConnected = connectToWiFi(userSSID, userPassword);
    }
    
    // Ensure AP mode is active
    if (!apModeActive) {
        setupAPMode();
    }
}
