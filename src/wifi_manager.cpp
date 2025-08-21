#include "wifi_manager.h"
#include <EEPROM.h>
#include <esp_wifi.h>

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
    lastWifiErrorCode(WL_IDLE_STATUS),
    setupUICallback(nullptr),
    initializeSensorCallback(nullptr),
    updateConnectionStatusCallback(nullptr),
    sendDataCallback(nullptr),
    startNewMeasurementCallback(nullptr),
    handleAIAnalysisCallback(nullptr)
{
    // Initialize common CSS
    commonCSS = "@import url('https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500;700&display=swap');"
                "body { font-family: 'Roboto', Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                "h1 { color: #333; }"
                ".status { font-weight: bold; margin-bottom: 20px; }"
                ".connected { color: #4CAF50; }"
                ".disconnected { color: #f44336; }"
                "button, input[type='submit'] { background: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 0; width: 100%; }"
                "button:hover, input[type='submit']:hover { background: #45a049; }"
                "input[type='text'], input[type='password'], input[type='email'] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }"
                ".guest-btn { background: #2196F3; }"
                ".guest-btn:hover { background: #0b7dda; }"
                ".back-btn { background: #f44336; }"
                ".back-btn:hover { background: #d32f2f; }";

    apIP = IPAddress(192, 168, 4, 1);
    server = new WebServer(80);
    dnsServer = new DNSServer();
}

void WiFiManager::begin() {
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.end();
    
    // Reset WiFi before anything else
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000);
    
    // Read saved WiFi credentials
    readWiFiCredentials();
    
    Serial.println("Starting WiFi Manager");
    Serial.print("SDK Version: ");
    Serial.println(ESP.getSdkVersion());

    // Configure low power settings (but not too aggressive)
    esp_wifi_set_ps(WIFI_PS_NONE); // Disable power saving for better responsiveness
    
    // Always start in AP mode first
    setupAPMode();
    
    // Try to connect to saved WiFi if credentials exist
    if (userSSID.length() > 0) {
        Serial.print("Attempting to connect to saved WiFi: '");
        Serial.print(userSSID);
        Serial.println("'");
        
        // Debug saved password length
        Serial.print("Password length: ");
        Serial.println(userPassword.length());
        
        isConnected = connectToWiFi(userSSID, userPassword);
        
        if (isConnected) {
            Serial.println("Auto-connected to saved WiFi network");
        } else {
            Serial.println("Failed to auto-connect to saved WiFi");
            Serial.println("Double check SSID and password");
        }
    }
    
    // Configure web server
    // Set higher timeouts to prevent connection issues
    server->enableCORS(true);
    server->enableCrossOrigin(true);
    
    // Setup web server routes
    server->on("/", [this](){ this->handleRoot(); });
    server->on("/wifi", [this](){ this->handleWiFi(); });
    server->on("/connect", HTTP_POST, [this](){ this->handleConnect(); });
    server->on("/mode", [this](){ this->handleModeSelect(); });
    server->on("/login", [this](){ this->handleLogin(); });
    server->on("/login_submit", HTTP_POST, [this](){ this->handleLoginSubmit(); });
    server->on("/guest", [this](){ this->handleGuest(); });
    server->on("/measurement", [this](){ this->handleMeasurement(); });
    server->on("/continue_measuring", [this](){ this->handleContinueMeasuring(); });
    server->on("/reconfigure_wifi", [this](){ this->handleReconfigWiFi(); });
    server->on("/status", [this](){ this->handleStatus(); });
    server->on("/force_ap", [this](){ this->handleForceAP(); });
    server->on("/ai_analysis", [this](){ this->handleAIAnalysis(); });
    server->on("/return_to_measurement", [this](){ this->handleReturnToMeasurement(); });
    
    // Add routes for common captive portal detection URLs
    server->on("/generate_204", [this](){ this->handleRoot(); }); // Android captive portal
    server->on("/mobile/status.php", [this](){ this->handleRoot(); }); // Another Android endpoint
    server->on("/hotspot-detect.html", [this](){ this->handleRoot(); }); // iOS captive portal
    server->on("/library/test/success.html", [this](){ this->handleRoot(); }); // iOS captive portal
    server->on("/favicon.ico", HTTP_GET, [this](){ server->send(200, "image/x-icon", ""); }); // Handle favicon requests
    
    // Last catch-all handler
    server->onNotFound([this](){ this->handleNotFound(); });
    
    // Start the server
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
    
    // Configure softAP with better parameters
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    
    // Use a more reliable AP setup with channel specification
    bool apSuccess = WiFi.softAP(ap_ssid, ap_password, 1, false, 4); // Channel 1, not hidden, max 4 connections
    
    if (!apSuccess) {
        Serial.println("Failed to setup AP mode - trying again with default parameters");
        WiFi.softAP(ap_ssid, ap_password);  // Try with default params
    }
    
    delay(500); // Small delay to ensure AP is fully set up
    
    if (setupUICallback) {
        setupUICallback();
    }
    
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    
    // Stop any existing DNS server and restart it
    dnsServer->stop();
    // Setup captive portal with more reliable parameters
    bool dnsStarted = dnsServer->start(53, "*", apIP);
    
    if (!dnsStarted) {
        Serial.println("Failed to start DNS server");
    } else {
        Serial.println("DNS server started successfully");
    }
    
    // Start MDNS responder
    if (MDNS.begin("healthsense")) {
        Serial.println("MDNS responder started");
    }
    
    apModeActive = true;
}

bool WiFiManager::connectToWiFi(String ssid, String password) {
    if (ssid.length() == 0) {
        Serial.println("Error: Empty SSID provided");
        return false;
    }
    
    Serial.println("Connecting to WiFi");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Password length: ");
    Serial.println(password.length());
    
    // Disconnect from any previous WiFi
    WiFi.disconnect(true);
    delay(1000);
    
    // Use dual mode to maintain AP while connecting to WiFi
    WiFi.mode(WIFI_AP_STA);
    
    // Fix for ESP32 WiFi connection issues
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    
    // Try to improve connection reliability
    esp_wifi_set_ps(WIFI_PS_NONE); // Disable power saving
    
    // Begin connection attempt
    Serial.println("Starting connection...");
    WiFi.begin(ssid.c_str(), password.c_str());
    
    // Debug connection status
    Serial.print("Initial connection status: ");
    Serial.println(WiFi.status());
    
    int attempts = 0;
    int maxAttempts = 45;  // Increased timeout (22.5 seconds)
    
    Serial.println("Waiting for connection...");
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
        delay(500);
        Serial.print(".");
        
        // Debug more frequently
        if (attempts % 3 == 0) {
            Serial.print(" [Status: ");
            Serial.print(WiFi.status());
            Serial.println("]");
        }
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected successfully");
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
        int wifiErrorCode = WiFi.status();
        Serial.println("");
        Serial.print("WiFi connection failed with status: ");
        Serial.println(wifiErrorCode);
        
        // Display error based on status code
        switch (wifiErrorCode) {
            case WL_NO_SSID_AVAIL:
                Serial.println("SSID not available - Check network name");
                break;
            case WL_CONNECT_FAILED:
                Serial.println("Invalid password or authentication failed");
                break;
            case WL_CONNECTION_LOST:
                Serial.println("Connection lost");
                break;
            default:
                Serial.println("Unknown error");
                break;
        }
        
        // If connection failed, ensure AP mode is still active
        if (!apModeActive) {
            setupAPMode();
        }
        
        // Pass the specific error code to the connection status callback
        if (updateConnectionStatusCallback) {
            // Store the error code for later use in status page
            lastWifiErrorCode = wifiErrorCode;
            updateConnectionStatusCallback(false, false, isLoggedIn);
        }
        
        if (updateConnectionStatusCallback) {
            updateConnectionStatusCallback(false, false, isLoggedIn);
        }
        
        return false;
    }
}

void WiFiManager::readWiFiCredentials() {
    EEPROM.begin(EEPROM_SIZE);
    
    // Read SSID (properly null-terminated)
    char ssid[65]; // +1 for null terminator
    for (int i = 0; i < 64; i++) {
        ssid[i] = EEPROM.read(SSID_ADDR + i);
    }
    ssid[64] = 0; // Ensure null-termination
    userSSID = String(ssid);
    
    // Read password (properly null-terminated)
    char password[65]; // +1 for null terminator
    for (int i = 0; i < 64; i++) {
        password[i] = EEPROM.read(PASS_ADDR + i);
    }
    password[64] = 0; // Ensure null-termination
    userPassword = String(password);
    
    // Read mode
    isGuestMode = (EEPROM.read(MODE_ADDR) == 1);
    
    // Read email (properly null-terminated)
    char email[65]; // +1 for null terminator
    for (int i = 0; i < 64; i++) {
        email[i] = EEPROM.read(EMAIL_ADDR + i);
    }
    email[64] = 0; // Ensure null-termination
    userEmail = String(email);
    
    // Read UID (properly null-terminated)
    char uid[65]; // +1 for null terminator
    for (int i = 0; i < 64; i++) {
        uid[i] = EEPROM.read(UID_ADDR + i);
    }
    uid[64] = 0; // Ensure null-termination
    userUID = String(uid);
    
    // Set login status based on UID
    isLoggedIn = (userUID.length() > 0 && !isGuestMode);
    
    // Debug
    Serial.println("Read WiFi Credentials from EEPROM");
    Serial.print("SSID: '");
    Serial.print(userSSID);
    Serial.print("', Password length: ");
    Serial.println(userPassword.length());
    Serial.print("Guest Mode: ");
    Serial.print(isGuestMode ? "YES" : "NO");
    Serial.print(", Logged In: ");
    Serial.println(isLoggedIn ? "YES" : "NO");
    
    EEPROM.end();
}

void WiFiManager::saveWiFiCredentials(String ssid, String password, bool guestMode) {
    EEPROM.begin(EEPROM_SIZE);
    
    // Debug
    Serial.print("Saving WiFi SSID: '");
    Serial.print(ssid);
    Serial.print("', Password length: ");
    Serial.println(password.length());
    
    // Save SSID (ensuring null termination)
    for (int i = 0; i < 64; i++) {
        if (i < ssid.length()) {
            EEPROM.write(SSID_ADDR + i, ssid[i]);
        } else {
            EEPROM.write(SSID_ADDR + i, 0); // Null terminate
        }
    }
    
    // Save password (ensuring null termination)
    for (int i = 0; i < 64; i++) {
        if (i < password.length()) {
            EEPROM.write(PASS_ADDR + i, password[i]);
        } else {
            EEPROM.write(PASS_ADDR + i, 0); // Null terminate
        }
    }
    
    // Save mode
    EEPROM.write(MODE_ADDR, guestMode ? 1 : 0);
    
    // Store the values in memory too
    userSSID = ssid;
    userPassword = password;
    isGuestMode = guestMode;
    
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
    
    if (!EEPROM.commit()) {
        Serial.println("ERROR: EEPROM commit failed");
    } else {
        Serial.println("WiFi credentials saved successfully");
    }
    
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
                  "<meta charset='UTF-8'>"
                  "<title>HealthSense WiFi Setup</title>"
                  "<style>" + commonCSS + "</style>"
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
                  "<meta charset='UTF-8'>"
                  "<title>HealthSense WiFi Setup</title>"
                  "<style>" + commonCSS + "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>Kết Nối WiFi</h1>"
                  "<form action='/connect' method='post'>"
                  "<label for='ssid'>Tên mạng WiFi:</label><br>"
                  "<input type='text' id='ssid' name='ssid' placeholder='Nhập tên WiFi' required><br>"
                  "<label for='password'>Mật khẩu WiFi:</label><br>"
                  "<input type='password' id='password' name='password' placeholder='Nhập mật khẩu'><br>"
                  "<input type='submit' value='Kết Nối'>"
                  "</form>"
                  "<form action='/' method='get'><button type='submit' class='back-btn'>Quay Lại</button></form>"
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
                    "<meta charset='UTF-8'>"
                    "<title>HealthSense WiFi Setup</title>"
                    "<style>" + commonCSS + 
                    ".success { color: #4CAF50; font-weight: bold; font-size: 18px; }"
                    ".error { color: #f44336; font-weight: bold; font-size: 18px; }"
                    "</style>"
                    "</head>"
                    "<body>"
                    "<div class='container'>"
                    "<h1>Kết Quả Kết Nối</h1>";
        
        // Try to connect to the WiFi
        Serial.println("Attempting WiFi connection from web interface...");
        Serial.print("SSID: '");
        Serial.print(ssid);
        Serial.print("', Password length: ");
        Serial.println(password.length());
        
        // Try connection twice in case of initial failure
        isConnected = connectToWiFi(ssid, password);
        
        // If first attempt failed, reset WiFi and try again
        if (!isConnected) {
            Serial.println("First connection attempt failed, trying again after reset...");
            WiFi.disconnect(true);
            delay(1000);
            isConnected = connectToWiFi(ssid, password);
        }
        
        if (isConnected) {
            html += "<p class='success'>✅ Kết nối WiFi thành công!</p>"
                    "<p>Đã kết nối tới mạng: <strong>" + ssid + "</strong></p>"
                    "<p>Địa chỉ IP: " + WiFi.localIP().toString() + "</p>"
                    "<p>Cường độ tín hiệu: " + String(WiFi.RSSI()) + " dBm</p>"
                    "<meta http-equiv='refresh' content='5;url=/mode'>"
                    "<p>Tự động chuyển đến trang chọn chế độ sau 5 giây...</p>";
            
            // Maintain dual mode (AP + STA) so hotspot remains available
            // This is already set in connectToWiFi function
        } else {
            html += "<p class='error'>❌ Kết nối WiFi thất bại!</p>";
            
            // Show specific error message based on status code
            switch (WiFi.status()) {
                case WL_NO_SSID_AVAIL:
                    html += "<p>Lỗi: Không tìm thấy mạng WiFi \"<strong>" + ssid + "</strong>\"</p>"
                           "<p>Vui lòng kiểm tra tên mạng và đảm bảo mạng đang hoạt động.</p>";
                    break;
                case WL_CONNECT_FAILED:
                    html += "<p>Lỗi: Sai mật khẩu hoặc xác thực thất bại</p>"
                           "<p>Vui lòng kiểm tra lại mật khẩu WiFi của bạn.</p>";
                    break;
                case WL_CONNECTION_LOST:
                    html += "<p>Lỗi: Mất kết nối trong quá trình thiết lập</p>"
                           "<p>Tín hiệu WiFi có thể quá yếu. Hãy di chuyển thiết bị đến gần router WiFi hơn.</p>";
                    break;
                case WL_DISCONNECTED:
                    html += "<p>Lỗi: Không thể kết nối - Router có thể đã từ chối kết nối</p>"
                           "<p>Hãy kiểm tra cài đặt router và đảm bảo nó cho phép thêm thiết bị mới.</p>";
                    break;
                default:
                    html += "<p>Mã lỗi: " + String(WiFi.status()) + "</p>"
                           "<p>Lỗi không xác định. Hãy thử khởi động lại thiết bị và router WiFi.</p>";
                    break;
            }
            
            html += "<p>Vui lòng kiểm tra lại thông tin kết nối</p>"
                    "<form action='/wifi' method='get'><button type='submit'>Thử lại</button></form>"
                    "<meta http-equiv='refresh' content='10;url=/wifi'>"
                    "<p>Tự động quay lại trang cấu hình WiFi sau 10 giây...</p>";
            
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
                  "<a href='https://iot.newnol.io.vn' target='_blank'>HealthSense Portal</a></p>"
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
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>HealthSense Monitor</h1>";
    
    if (isLoggedIn) {
        html += "<p class='user-status'>User Mode - Data is being saved to server</p>";
    } else {
        html += "<p class='guest-status'>Guest Mode - Data is not being saved</p>"
                "<p>Register at: <a href='https://iot.newnol.io.vn' target='_blank'>HealthSense Portal</a></p>";
    }
    
    html += "<p class='status'>Place your finger on the sensor to start measuring</p>"
            "<p class='status'>Need 5 valid readings for final result</p>"
            "<div class='reading hr'>Heart Rate: <span id='hr'>--</span> BPM</div>"
            "<div class='reading spo2'>SpO2: <span id='spo2'>--</span> %</div>"
            "<div class='reading'>Progress: <span id='progress'>0/5</span> readings</div>";
    
    // Add continue measuring and AI analysis buttons
    html += "<div style='margin: 20px 0;'>"
            "<form action='/continue_measuring' method='get' style='display: inline;'>"
            "<button type='submit' style='background: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 5px;'>Start New Measurement</button>"
            "</form>"
            "<form action='/ai_analysis' method='get' style='display: inline;'>"
            "<button type='submit' style='background: #2196F3; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 5px;'>AI Analysis</button>"
            "</form>"
            "</div>";
    
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

void WiFiManager::handleContinueMeasuring() {
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>HealthSense New Measurement</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                  ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                  "h1 { color: #333; }"
                  ".success { color: #4CAF50; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>New Measurement Started</h1>"
                  "<p class='success'>Starting new measurement cycle...</p>"
                  "<p>Place your finger on the sensor and keep it steady.</p>"
                  "<p>The system will collect 5 valid readings and calculate the average.</p>"
                  "<meta http-equiv='refresh' content='3;url=/measurement'>"
                  "<p>You will be redirected to measurement page in 3 seconds...</p>"
                  "</div>"
                  "</body></html>";
    
    server->send(200, "text/html", html);
    
    // Trigger start of new measurement via callback
    Serial.println("Web interface requested new measurement cycle");
    
    if (startNewMeasurementCallback) {
        startNewMeasurementCallback();
    }
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
                  "<meta charset='UTF-8'>"
                  "<title>HealthSense Connection Status</title>"
                  "<style>" + commonCSS + 
                  ".status-info { text-align: left; background: #f9f9f9; padding: 15px; border-radius: 5px; margin: 15px 0; font-family: monospace; }"
                  ".error-banner { background-color: #ffebee; color: #d32f2f; padding: 10px; border-radius: 5px; border: 1px solid #ffcdd2; margin: 15px 0; }"
                  ".success-banner { background-color: #e8f5e9; color: #388e3c; padding: 10px; border-radius: 5px; border: 1px solid #c8e6c9; margin: 15px 0; }"
                  ".refresh-btn { background: #FF9800; }"
                  ".refresh-btn:hover { background: #e68900; }"
                  ".try-again-btn { background: #9c27b0; }"
                  ".try-again-btn:hover { background: #7b1fa2; }"
                  ".debug-section { margin-top: 20px; border-top: 1px dashed #ccc; padding-top: 15px; }"
                  ".debug-title { font-weight: bold; color: #555; }"
                  ".debug-info { font-family: monospace; background: #eee; padding: 10px; border-radius: 3px; text-align: left; white-space: pre-wrap; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>Connection Status</h1>";
                  
    // Add connection status banner
    if (isConnected) {
        html += "<div class='success-banner'><strong>✅ WiFi Connected</strong> to " + userSSID + "</div>";
    } else {
        html += "<div class='error-banner'><strong>❌ WiFi Disconnected</strong> - ";
        
        // Show specific error based on WiFi status
        switch (lastWifiErrorCode) {
            case WL_NO_SSID_AVAIL:
                html += "Network \"" + userSSID + "\" not found!";
                break;
            case WL_CONNECT_FAILED:
                html += "Invalid password or authentication failed.";
                break;
            case WL_CONNECTION_LOST:
                html += "Connection lost during setup.";
                break;
            default:
                html += "Error code: " + String(lastWifiErrorCode);
                break;
        }
        html += "</div>";
    }
                  
    html += "<div class='status-info'>";
    
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
    
    if (!isConnected) {
        html += "<form action='/wifi' method='get' style='display: inline;'><button type='submit' class='try-again-btn'>Try WiFi Setup Again</button></form>";
    }
    
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
    // Enhanced captive portal handling
    Serial.print("Handling not found request for URI: ");
    Serial.println(server->uri());

    // Special handling for Apple devices captive portal detection
    if (server->hostHeader() == "captive.apple.com") {
        Serial.println("Apple captive portal detection - redirecting to success page");
        server->send(200, "text/html", "<!DOCTYPE html><html><head><title>Success</title></head><body>Success</body></html>");
        return;
    }
    
    // For Android captive portal detection
    if (server->hostHeader() == "connectivitycheck.gstatic.com" || 
        server->hostHeader() == "connectivitycheck.android.com" ||
        server->hostHeader() == "clients3.google.com") {
        Serial.println("Android/Google captive portal detection - generating redirect");
        server->send(200, "text/html", "<!DOCTYPE html><html><head><title>Success</title></head><body>Success</body></html>");
        return;
    }

    // For all other requests, redirect to our web interface
    String contentType = "text/html";
    
    // Respond with a simple HTML page for browsers
    String message = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    message += "<meta http-equiv='refresh' content='0;url=http://" + apIP.toString() + "/'>";
    message += "<title>Redirecting...</title></head>";
    message += "<body>Redirecting to <a href='http://" + apIP.toString() + "/'>HealthSense Setup</a>...</body></html>";
    
    // Set headers to ensure the redirect works and isn't cached
    server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server->sendHeader("Pragma", "no-cache");
    server->sendHeader("Expires", "-1");
    server->sendHeader("Location", "http://" + apIP.toString() + "/", true);
    
    // Send 302 status with the HTML page as fallback
    server->send(302, contentType, message);
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

void WiFiManager::setStartNewMeasurementCallback(void (*callback)()) {
    startNewMeasurementCallback = callback;
}

void WiFiManager::setHandleAIAnalysisCallback(void (*callback)(String summary)) {
    handleAIAnalysisCallback = callback;
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
    String url = serverURL;
    if (!url.endsWith("/")) {
        url += "/";
    }
    url += "api/login";
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


bool WiFiManager::sendDeviceData(int32_t heartRate, int32_t spo2, String userId) {
    if (!isConnected) {
        Serial.println(F("❌ Not connected to WiFi, cannot send data"));
        return false;
    }
    
    Serial.println(F("🌐 Preparing to send device data..."));
    
    HTTPClient http;
    String url = serverURL;
    if (!url.endsWith("/")) {
        url += "/";
    }
    url += "api/records";
    
    Serial.print(F("📍 URL: "));
    Serial.println(url);
    
    // Use a try-catch-like approach for safer execution
    bool initSuccess = false;
    try {
        initSuccess = http.begin(url);
    } catch (...) {
        Serial.println(F("❌ Failed to initialize HTTP client"));
        return false;
    }
    
    if (!initSuccess) {
        Serial.println(F("❌ HTTP client initialization failed"));
        return false;
    }
    
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Id", DEVICE_ID);
    http.addHeader("X-Device-Secret", DEVICE_SECRET);
    
    // Add user ID header if provided
    if (userId.length() > 0) {
        http.addHeader("X-User-Id", userId);
        Serial.print(F("👤 User ID: "));
        Serial.println(userId);
    }
    
    // Create JSON payload with correct field names
    DynamicJsonDocument doc(200);
    doc["heart_rate"] = heartRate;
    doc["spo2"] = spo2;
    String payload;
    serializeJson(doc, payload);
    
    Serial.print(F("📦 Payload: "));
    Serial.println(payload);
    
    // Send POST request with timeout
    int httpCode = -1;
    try {
        http.setTimeout(10000); // 10 second timeout
        httpCode = http.POST(payload);
    } catch (...) {
        Serial.println(F("❌ HTTP POST request failed (exception)"));
        http.end();
        return false;
    }
    
    Serial.print(F("📡 Response code: "));
    Serial.println(httpCode);
    
    bool success = false;
    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        Serial.print(F("✅ Success! Response: "));
        Serial.println(response);
        success = true;
    } else if (httpCode > 0) {
        String response = http.getString();
        Serial.print(F("❌ HTTP error "));
        Serial.print(httpCode);
        Serial.print(F(". Response: "));
        Serial.println(response);
    } else {
        Serial.print(F("❌ Connection error: "));
        Serial.println(http.errorToString(httpCode));
    }
    
    http.end();
    Serial.println(F("🔚 HTTP client closed"));
    return success;
}


void WiFiManager::sendSensorData(int32_t heartRate, int32_t spo2) {
    Serial.println(F("🔄 sendSensorData() called"));
    Serial.print(F("Parameters - HR: "));
    Serial.print(heartRate);
    Serial.print(F(", SpO2: "));
    Serial.println(spo2);
    Serial.print(F("State - isMeasuring: "));
    Serial.print(isMeasuring);
    Serial.print(F(", isLoggedIn: "));
    Serial.print(isLoggedIn);
    Serial.print(F(", isGuestMode: "));
    Serial.print(isGuestMode);
    Serial.print(F(", userUID length: "));
    Serial.println(userUID.length());
    
    // Only send data if measuring and user is logged in (not guest mode)
    if (isMeasuring && isLoggedIn && !isGuestMode && userUID.length() > 0) {
        Serial.println(F("📤 Sending measurement data to server (User mode)"));
        bool success = sendDeviceData(heartRate, spo2, userUID);
        if (success) {
            Serial.println(F("✅ Data sent successfully"));
        } else {
            Serial.println(F("❌ Failed to send data"));
        }
        
        // Call callback if it exists
        if (sendDataCallback) {
            Serial.println(F("🔔 Calling sendDataCallback for user mode"));
            sendDataCallback(userUID, heartRate, spo2);
        }
    } else if (isMeasuring && isGuestMode) {
        Serial.println(F("👤 Guest mode - not sending data to server"));
        
        // Call callback for guest mode (for local display only)
        if (sendDataCallback) {
            Serial.println(F("🔔 Calling sendDataCallback for guest mode"));
            sendDataCallback("guest", heartRate, spo2);
        }
    } else if (isMeasuring && !isLoggedIn) {
        Serial.println(F("🔒 User not logged in - not sending data to server"));
        
        // Call callback for anonymous mode (for local display only)
        if (sendDataCallback) {
            Serial.println(F("🔔 Calling sendDataCallback for anonymous mode"));
            sendDataCallback("anonymous", heartRate, spo2);
        }
    } else {
        Serial.println(F("⚠️ Conditions not met for sending data"));
        Serial.print(F("  - isMeasuring: "));
        Serial.println(isMeasuring ? "true" : "false");
        Serial.print(F("  - isLoggedIn: "));
        Serial.println(isLoggedIn ? "true" : "false");
        Serial.print(F("  - isGuestMode: "));
        Serial.println(isGuestMode ? "true" : "false");
        Serial.print(F("  - userUID: '"));
        Serial.print(userUID);
        Serial.println(F("'"));
    }
    
    // Reset measurement state after data is sent
    isMeasuring = false;
    Serial.println(F("🛑 Measurement state reset - ready for new measurement"));
    
    Serial.println(F("🏁 sendSensorData() completed"));
}

bool WiFiManager::getAIHealthSummary(String& summary) {
    if (!isConnected) {
        Serial.println(F("❌ Not connected to WiFi, cannot get AI summary"));
        return false;
    }
    
    Serial.println(F("🧠 Requesting AI health summary..."));
    
    HTTPClient http;
    String url = serverURL;
    if (!url.endsWith("/")) {
        url += "/";
    }
    url += "api/ai/sumerize";
    
    Serial.print(F("📍 URL: "));
    Serial.println(url);
    
    bool initSuccess = false;
    try {
        initSuccess = http.begin(url);
    } catch (...) {
        Serial.println(F("❌ Failed to initialize HTTP client"));
        return false;
    }
    
    if (!initSuccess) {
        Serial.println(F("❌ HTTP client initialization failed"));
        return false;
    }
    
    // Add headers
    http.addHeader("X-Device-Id", DEVICE_ID);
    
    // Add user ID if not in guest mode and user is logged in
    if (!isGuestMode && isLoggedIn && userUID.length() > 0) {
        http.addHeader("X-User-Id", userUID);
    }
    
    // Free some memory before making the request
    ESP.getFreeHeap(); // This call helps clear internal heap fragmentation
    Serial.print(F("📈 Free memory before request: "));
    Serial.println(ESP.getFreeHeap());
    
    // Send GET request to get AI summary
    int httpCode = http.GET();
    Serial.print(F("📥 AI Summary API response code: "));
    Serial.println(httpCode);
    
    bool success = false;
    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        Serial.println(F("✅ AI summary received successfully"));
        Serial.print(F("📊 Response length: "));
        Serial.println(response.length());
        
        // Log part of the response for debugging
        Serial.print(F("🔍 Response preview: "));
        if (response.length() > 100) {
            Serial.println(response.substring(0, 100) + "...");
        } else {
            Serial.println(response);
        }
        
        // Parse JSON response with increased buffer size
        DynamicJsonDocument doc(4096); // Increased buffer for AI summary (4KB)
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error) {
            summary = doc["summary"].as<String>();
            success = true;
            Serial.println(F("✅ JSON parsed successfully"));
        } else {
            Serial.print(F("❌ JSON parsing error: "));
            Serial.println(error.c_str());
            Serial.println(F("💡 Try increasing DynamicJsonDocument size if NoMemory error persists"));
            
            // In case of memory error, try a crude extraction as fallback
            if (error == DeserializationError::NoMemory) {
                Serial.println(F("🔄 Attempting fallback parsing method"));
                
                // Simple string extraction (looking for "summary": "text")
                int summaryStart = response.indexOf("\"summary\":");
                if (summaryStart > 0) {
                    summaryStart = response.indexOf("\"", summaryStart + 10) + 1;
                    int summaryEnd = response.indexOf("\"", summaryStart);
                    if (summaryStart > 0 && summaryEnd > summaryStart) {
                        summary = response.substring(summaryStart, summaryEnd);
                        Serial.println(F("✅ Fallback parsing succeeded"));
                        success = true;
                    } else {
                        summary = "Error: Could not parse AI summary (fallback failed)";
                        success = false;
                    }
                } else {
                    summary = "Error: Could not parse AI summary (no summary field)";
                    success = false;
                }
            } else {
                summary = "Error: Could not parse AI summary";
                success = false;
            }
        }
    } else {
        Serial.print(F("❌ Failed to get AI summary: "));
        Serial.println(http.errorToString(httpCode));
        summary = "Error: Failed to get AI summary (Code: " + String(httpCode) + ")";
        success = false;
    }
    
    http.end();
    Serial.println(F("🔚 HTTP client closed"));
    
    // Check memory after request
    Serial.print(F("📉 Free memory after request: "));
    Serial.println(ESP.getFreeHeap());
    
    return success;
}

bool WiFiManager::requestAIHealthSummary(String& summary) {
    Serial.println(F("🔄 requestAIHealthSummary() called"));
    
    if (!isConnected) {
        summary = "Error: No WiFi connection";
        Serial.println(F("❌ Not connected to WiFi"));
        return false;
    }
    
    bool success = getAIHealthSummary(summary);
    
    if (success) {
        Serial.println(F("✅ AI health summary obtained successfully"));
    } else {
        Serial.println(F("❌ Failed to get AI health summary"));
        if (summary.isEmpty()) {
            summary = "Error: Unable to retrieve health analysis";
        }
    }
    
    Serial.println(F("🏁 requestAIHealthSummary() completed"));
    return success;
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
        info += "- MAC Address: " + WiFi.macAddress() + "\n";
        info += "- Signal Strength: " + String(WiFi.RSSI()) + " dBm\n";
        info += "- DNS Server: " + WiFi.dnsIP().toString() + "\n";
    } else {
        info += "- Not connected to WiFi\n";
        info += "- Last Error Code: " + String(lastWifiErrorCode) + "\n";
        
        // Add specific error descriptions
        switch (lastWifiErrorCode) {
            case WL_NO_SSID_AVAIL:
                info += "  (SSID not available - check network name)\n";
                break;
            case WL_CONNECT_FAILED:
                info += "  (Connection failed - check password)\n";
                break;
            case WL_DISCONNECTED:
                info += "  (Disconnected or unable to connect)\n";
                break;
            case WL_CONNECTION_LOST:
                info += "  (Connection was lost)\n";
                break;
        }
    }
    
    if (apModeActive) {
        info += "- Hotspot Active: " + String(ap_ssid) + "\n";
        info += "- Hotspot IP: " + WiFi.softAPIP().toString() + "\n";
        info += "- AP MAC Address: " + WiFi.softAPmacAddress() + "\n";
        info += "- Connected Clients: " + String(WiFi.softAPgetStationNum()) + "\n";
    } else {
        info += "- Hotspot: Inactive\n";
    }
    
    // Add system info
    info += "\nSystem Info:\n";
    info += "- Free Memory: " + String(ESP.getFreeHeap()) + " bytes\n";
    info += "- SDK Version: " + String(ESP.getSdkVersion()) + "\n";
    
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

void WiFiManager::handleAIAnalysis() {
    // If not in guest mode and not logged in, redirect to mode selection
    if (!isGuestMode && !isLoggedIn) {
        server->sendHeader("Location", "/mode");
        server->send(302, "text/plain", "");
        return;
    }
    
    // Request AI health summary
    String aiSummary;
    bool success = requestAIHealthSummary(aiSummary);
    
    if (!success) {
        aiSummary = "Error: Unable to retrieve AI health analysis. Please check your connection and try again.";
    }
    
    // Create HTML response
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>HealthSense AI Analysis</title>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f0f0f0; }"
                  ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
                  "h1 { color: #333; margin-bottom: 30px; }"
                  ".header { background: linear-gradient(to right, #003366, #0066cc); color: white; padding: 15px; border-radius: 10px 10px 0 0; margin: -20px -20px 20px; }"
                  ".summary { text-align: left; line-height: 1.6; padding: 15px; background-color: #f9f9f9; border-radius: 5px; border-left: 5px solid #2196F3; margin-bottom: 30px; }"
                  "button, input[type='submit'] { background: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 5px; font-weight: bold; }"
                  "button:hover, input[type='submit']:hover { background: #45a049; }"
                  ".back-btn { background: #2196F3; }"
                  ".back-btn:hover { background: #0b7dda; }"
                  ".disclaimer { font-size: 12px; color: #757575; margin-top: 30px; font-style: italic; }"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<div class='header'>"
                  "<h1>AI Health Summary</h1>"
                  "</div>"
                  "<div class='summary'>" + aiSummary + "</div>"
                  "<div>"
                  "<form action='/measurement' method='get' style='display: inline;'>"
                  "<button type='submit' class='back-btn'>Back to Measurements</button>"
                  "</form>"
                  "<form action='/return_to_measurement' method='get' style='display: inline;'>"
                  "<button type='submit'>New Measurement</button>"
                  "</form>"
                  "</div>"
                  "<p class='disclaimer'>This analysis is provided for informational purposes only and should not replace professional medical advice.</p>"
                  "</div>"
                  "</body></html>";
    
    server->send(200, "text/html", html);
    
    // Display the AI health summary on the device using the callback
    if (handleAIAnalysisCallback) {
        handleAIAnalysisCallback(aiSummary);
    }
}

void WiFiManager::handleReturnToMeasurement() {
    // Reset measurement state
    isMeasuring = true;
    
    // Trigger sensor initialization (this also resets the display)
    if (initializeSensorCallback) {
        initializeSensorCallback();
    }
    
    // The AppState handling is now done in initializeSensorCallback()
    
    // Start new measurement if callback exists
    if (startNewMeasurementCallback) {
        startNewMeasurementCallback();
    }
    
    // Redirect to measurement page
    server->sendHeader("Location", "/measurement");
    server->send(302, "text/plain", "");
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
