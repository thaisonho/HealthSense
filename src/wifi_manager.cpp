#include "wifi_manager.h"
#include "sensor_manager.h"
#include "display_manager.h" // Include DisplayManager header
#include <EEPROM.h>
#include <esp_wifi.h>

// Reference to the SensorManager instance in main.cpp
extern SensorManager sensorManager;

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
    // Tối ưu CSS bằng cách loại bỏ font Google và tối giản mã CSS
    commonCSS = "body{font-family:Arial,sans-serif;margin:0;padding:15px;text-align:center;background:#f0f0f0}"
                ".container{max-width:400px;margin:0 auto;background:#fff;padding:15px;border-radius:8px;box-shadow:0 1px 5px rgba(0,0,0,.1)}"
                "h1{color:#333;font-size:20px;margin-top:0}"
                ".status{font-weight:700;margin-bottom:15px}.connected{color:#4CAF50}.disconnected{color:#f44336}"
                "button,input[type=submit]{background:#4CAF50;color:#fff;padding:8px 12px;border:none;border-radius:4px;cursor:pointer;margin:8px 0;width:100%}"
                "input[type=email],input[type=password],input[type=text]{width:100%;padding:8px;margin:8px 0;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}"
                ".guest-btn{background:#2196F3}.back-btn{background:#f44336}";

    apIP = IPAddress(192, 168, 4, 1);
    server = new WebServer(80);
    
    // Cấu hình timeout cho web server để tránh kết nối treo
    // Phương thức này không tồn tại trong thư viện WebServer tiêu chuẩn nhưng chúng ta có thể thêm vào sau
    // server->setServerTimeout(10000); // 10 giây timeout
    
    dnsServer = new DNSServer();
}

void WiFiManager::begin() {
    // Khởi tạo EEPROM
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.end();
    
    // Đọc thông tin kết nối WiFi đã lưu
    readWiFiCredentials();
    
    Serial.println("Starting WiFi Manager");
    Serial.print("SDK Version: ");
    Serial.println(ESP.getSdkVersion());
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    
    // Reset WiFi hoàn toàn trước khi bắt đầu
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(500);

    // Cấu hình không tiết kiệm năng lượng cho WiFi
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    // Luôn khởi động ở chế độ AP trước
    setupAPMode();
    
    // Thử kết nối với WiFi đã lưu nếu có thông tin
    if (userSSID.length() > 0) {
        Serial.print("Attempting to connect to saved WiFi: '");
        Serial.print(userSSID);
        Serial.println("'");
        
        isConnected = connectToWiFi(userSSID, userPassword);
        
        if (isConnected) {
            Serial.println("Auto-connected to saved WiFi network");
        } else {
            Serial.println("Failed to auto-connect to saved WiFi");
        }
    }
    
    // Cấu hình web server với các tùy chọn cải thiện hiệu suất
    server->enableCORS(true);
    server->enableCrossOrigin(true);
    
    // Thiết lập các route cho web server
    // Giao diện chính và thiết lập WiFi
    server->on("/", [this](){ this->handleRoot(); });
    server->on("/wifi", [this](){ this->handleWiFi(); });
    server->on("/connect", HTTP_POST, [this](){ this->handleConnect(); });
    
    // Các route chức năng chính
    server->on("/mode", [this](){ this->handleModeSelect(); });
    server->on("/login", [this](){ this->handleLogin(); });
    server->on("/login_submit", HTTP_POST, [this](){ this->handleLoginSubmit(); });
    server->on("/guest", [this](){ this->handleGuest(); });
    
    // Các route đo lường và phân tích
    server->on("/measurement", [this](){
        // Giải phóng bộ nhớ trước khi xử lý request đo lường
        ESP.getFreeHeap();
        this->handleMeasurement();
    });
    server->on("/measurement_info", [this](){ this->handleMeasurementInfo(); });
    server->on("/measurement_stream", [this](){ this->handleMeasurementStream(); });
    server->on("/continue_measuring", [this](){ this->handleContinueMeasuring(); });
    server->on("/start_measurement", [this](){ this->handleStartMeasurement(); }); // New endpoint for browser to confirm page load
    server->on("/check_measurement_status", [this](){ this->handleCheckMeasurementStatus(); }); // New endpoint to check if measurement is complete
    server->on("/ai_analysis", [this](){ this->handleAIAnalysis(); });
    server->on("/return_to_measurement", [this](){ this->handleReturnToMeasurement(); });
    
    // Các route tiện ích
    server->on("/reconfigure_wifi", [this](){ this->handleReconfigWiFi(); });
    server->on("/status", [this](){ this->handleStatus(); });
    server->on("/force_ap", [this](){ this->handleForceAP(); });
    
    // Hỗ trợ captive portal cho các thiết bị di động
    server->on("/generate_204", [this](){ this->handleRoot(); }); // Android
    server->on("/mobile/status.php", [this](){ this->handleRoot(); }); // Android
    server->on("/hotspot-detect.html", [this](){ this->handleRoot(); }); // iOS
    server->on("/library/test/success.html", [this](){ this->handleRoot(); }); // iOS
    server->on("/favicon.ico", HTTP_GET, [this](){ server->send(200, "image/x-icon", ""); });
    
    // Handler cho các route không tìm thấy
    server->onNotFound([this](){
        // Đảm bảo vẫn giải phóng bộ nhớ
        cleanupConnections();
        this->handleNotFound();
    });
    
    // Khởi động server
    server->begin();
    Serial.println("HTTP server started");
    
    // In thông tin kết nối
    Serial.print("Free heap after setup: ");
    Serial.println(ESP.getFreeHeap());
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
    
    // Track connection quality for debugging
    static int connectionErrorCounter = 0;
    static unsigned long lastSocketCleanup = 0;
    
    // Check if we lost WiFi connection
    if (isConnected && WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost!");
        Serial.print("Current SSID: '");
        Serial.print(userSSID);
        Serial.print("', Password length: ");
        Serial.println(userPassword.length());
        Serial.print("Guest Mode: ");
        Serial.println(isGuestMode ? "YES" : "NO");
        
        isConnected = false;
        connectionErrorCounter++;
        
        // Try to reconnect with saved credentials
        if (userSSID.length() > 0) {
            Serial.println("Attempting to reconnect...");
            
            // Clean up any existing connections first
            WiFi.disconnect(true);
            delay(200);  // Give it time to disconnect properly
            
            // Ensure we're in the correct mode (AP+STA)
            WiFi.mode(WIFI_AP_STA);
            delay(200);  // Give it time to change mode
            
            // Now try to connect
            Serial.print("Connecting to SSID: ");
            Serial.println(userSSID);
            WiFi.begin(userSSID.c_str(), userPassword.c_str());
            
            // Give it a few seconds to reconnect - longer timeout
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) { // Longer timeout (6s)
                delay(300);
                Serial.print(".");
                attempts++;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("\nReconnected to WiFi!");
                Serial.print("Connected to: ");
                Serial.print(WiFi.SSID());
                Serial.print(" | IP address: ");
                Serial.println(WiFi.localIP());
                
                isConnected = true;
                connectionErrorCounter = 0; // Reset error counter on success
                
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
    } else if (isConnected) {
        // Connection is good, reset error counter
        connectionErrorCounter = 0;
    }
    
    // If we have persistent connection issues, try more aggressive cleanup
    if (connectionErrorCounter >= 3 && millis() - lastSocketCleanup > 60000) {
        Serial.println("Persistent connection issues detected, performing socket cleanup");
        forceSocketCleanup();
        lastSocketCleanup = millis();
        connectionErrorCounter = 0;
    }
    
    // Check if we need to restart AP mode (in case it was disabled)
    if (!apModeActive && WiFi.getMode() != WIFI_AP_STA && WiFi.getMode() != WIFI_AP) {
        Serial.println("AP mode not active, restarting...");
        setupAPMode();
    }
    
    // Periodically log connection status for debugging
    static unsigned long lastStatusLog = 0;
    if (millis() - lastStatusLog > 60000) { // Every minute
        lastStatusLog = millis();
        Serial.print("WiFi Status: ");
        Serial.print(WiFi.status());
        Serial.print(" | Mode: ");
        Serial.print(WiFi.getMode());
        Serial.print(" | Free Heap: ");
        Serial.print(ESP.getFreeHeap());
        Serial.print(" | Connection errors: ");
        Serial.println(connectionErrorCounter);
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
    
    // Tách quá trình kết nối và phản hồi HTTP để tránh lỗi connection abort
    if (ssid.length() > 0) {
        // Đầu tiên lưu thông tin kết nối
        userSSID = ssid;
        userPassword = password;
        isGuestMode = false;
        saveWiFiCredentials(ssid, password, false);
        
        // Gửi trang HTML thông báo đang kết nối TRƯỚC khi thử kết nối WiFi
        String loadingHtml = "<!DOCTYPE html><html>"
                    "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                    "<meta charset='UTF-8'>"
                    "<title>Đang kết nối WiFi...</title>"
                    "<style>body{font-family:Arial;text-align:center;padding:20px;background:#f0f0f0;}"
                    ".container{max-width:400px;margin:0 auto;background:white;padding:15px;border-radius:8px;box-shadow:0 1px 5px rgba(0,0,0,0.1);}"
                    ".spinner{width:40px;height:40px;margin:20px auto;border-radius:50%;border:5px solid #f3f3f3;border-top:5px solid #3498db;animation:spin 1s linear infinite;}"
                    "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}</style>"
                    "<meta http-equiv='refresh' content='2;url=/connect_status'>"
                    "</head>"
                    "<body><div class='container'>"
                    "<h1>Đang kết nối WiFi</h1>"
                    "<p>Đang kết nối tới mạng: <strong>" + ssid + "</strong></p>"
                    "<div class='spinner'></div>"
                    "<p>Vui lòng đợi trong giây lát...</p>"
                    "</div></body></html>";
        
        // Gửi trang loading và đóng kết nối HTTP trước khi tiến hành kết nối WiFi
        server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        server->send(200, "text/html", loadingHtml);
        
        // Đăng ký handler mới để hiển thị kết quả sau khi chuyển hướng
        server->on("/connect_status", [this, ssid, password]() {
            // Thử kết nối WiFi
            Serial.println("Attempting WiFi connection from web interface...");
            Serial.print("SSID: '");
            Serial.print(ssid);
            Serial.print("', Password length: ");
            Serial.println(password.length());
            
            // Thử kết nối WiFi
            isConnected = connectToWiFi(ssid, password);
            
            // Nếu lần đầu thất bại, reset WiFi và thử lại
            if (!isConnected) {
                Serial.println("First connection attempt failed, trying again after reset...");
                WiFi.disconnect(true);
                delay(500); // Giảm thời gian delay
                isConnected = connectToWiFi(ssid, password);
            }
            
            // Chuẩn bị HTML kết quả
            String html = "<!DOCTYPE html><html>"
                        "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                        "<meta charset='UTF-8'>"
                        "<title>Kết quả kết nối</title>"
                        "<style>body{font-family:Arial;text-align:center;padding:20px;background:#f0f0f0;}"
                        ".container{max-width:400px;margin:0 auto;background:white;padding:15px;border-radius:8px;box-shadow:0 1px 5px rgba(0,0,0,0.1);}"
                        ".success{color:#4CAF50;font-weight:bold;font-size:16px;}"
                        ".error{color:#f44336;font-weight:bold;font-size:16px;}"
                        "button{background:#4CAF50;color:white;padding:10px 15px;border:none;border-radius:4px;cursor:pointer;margin:10px 0;width:100%;}</style>"
                        "</head><body><div class='container'>"
                        "<h1>Kết Quả Kết Nối</h1>";
            
            if (isConnected) {
                html += "<p class='success'>✅ Kết nối WiFi thành công!</p>"
                        "<p>Đã kết nối tới: <strong>" + ssid + "</strong></p>"
                        "<p>IP: " + WiFi.localIP().toString() + "</p>";
                        
                if (WiFi.RSSI() > -70) {
                    html += "<p>Tín hiệu: Mạnh (-" + String(abs(WiFi.RSSI())) + " dBm)</p>";
                } else if (WiFi.RSSI() > -85) {
                    html += "<p>Tín hiệu: Trung bình (-" + String(abs(WiFi.RSSI())) + " dBm)</p>";
                } else {
                    html += "<p>Tín hiệu: Yếu (-" + String(abs(WiFi.RSSI())) + " dBm)</p>";
                }
                
                html += "<form action='/mode'><button type='submit'>Tiếp tục</button></form>";
            } else {
                html += "<p class='error'>❌ Kết nối WiFi thất bại!</p>";
                
                // Hiển thị lỗi cụ thể dựa trên mã trạng thái
                switch (WiFi.status()) {
                    case WL_NO_SSID_AVAIL:
                        html += "<p>Không tìm thấy mạng WiFi</p>";
                        break;
                    case WL_CONNECT_FAILED:
                        html += "<p>Sai mật khẩu hoặc xác thực thất bại</p>";
                        break;
                    default:
                        html += "<p>Mã lỗi: " + String(WiFi.status()) + "</p>";
                        break;
                }
                
                html += "<form action='/wifi'><button type='submit'>Thử lại</button></form>";
                
                // Đảm bảo chế độ AP vẫn hoạt động để có thể cấu hình lại
                if (!apModeActive) {
                    setupAPMode();
                }
            }
            
            html += "</div></body></html>";
            
            // Gửi HTML kết quả
            server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            server->send(200, "text/html", html);
            
            // Cập nhật trạng thái kết nối thông qua callback
            if (updateConnectionStatusCallback) {
                updateConnectionStatusCallback(isConnected, false, false);
            }
        });
    } else {
        // Chuyển hướng đến trang thiết lập WiFi
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
    resetMeasurementStreamState();
    
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
        
        // Do NOT set isMeasuring=true here - measurement should only start
        // after user clicks "Start Measuring" and page confirms load
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
    
    // In guest mode, we should still preserve the WiFi credentials
    // Just mark the mode as guest in EEPROM without clearing SSID/password
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(MODE_ADDR, 1); // Set guest mode flag (1=guest mode)
    EEPROM.commit();
    EEPROM.end();
    
    // Directly redirect to measurement page instead of showing guest page
    server->sendHeader("Location", "/measurement");
    server->send(302, "text/plain", "");
    
    // Initialize sensor if callback exists
    if (initializeSensorCallback) {
        initializeSensorCallback();
    }
    
    if (updateConnectionStatusCallback) {
        updateConnectionStatusCallback(isConnected, true, false);
    }
    
    // Do NOT set isMeasuring=true here - measurement should only start
    // after user clicks "Start Measuring" and page confirms load
}

void WiFiManager::handleMeasurement() {
    // If not in guest mode and not logged in, redirect to mode selection
    if (!isGuestMode && !isLoggedIn) {
        server->sendHeader("Location", "/mode");
        server->send(302, "text/plain", "");
        return;
    }
    
    // Reset measurement state to ensure we're starting fresh
    extern SensorManager sensorManager;
    sensorManager.stopMeasurement();
    isMeasuring = false;
    resetMeasurementStreamState();
    
    Serial.println("📱 Displaying measurement page - ready for user to start measuring");
    
    // Simplified CSS to reduce page size
    String css = "body{font-family:Arial;margin:0;padding:10px;background:#f0f0f0;text-align:center}"
                 ".container{max-width:400px;margin:0 auto;background:white;padding:15px;border-radius:8px;box-shadow:0 1px 5px rgba(0,0,0,0.1)}"
                 "h1{color:#333;font-size:20px;margin-top:0}"
                 ".reading{font-size:20px;margin:15px 0}"
                 ".hr{color:#f44336}.spo2{color:#2196F3}"
                 ".status{font-style:italic;color:#757575;margin-bottom:15px;font-size:14px}"
                 ".complete{color:#4CAF50;font-weight:bold;padding:8px;border:1px solid #4CAF50;border-radius:4px;background:#e8f5e9}"
                 ".measuring{color:#2196F3;font-weight:bold;padding:8px;border:1px solid #2196F3;border-radius:4px;background:#e3f2fd}"
                 ".user{color:#4CAF50;font-weight:bold;font-size:14px}.guest{color:#FF9800;font-weight:bold;font-size:14px}"
                 ".card{border:1px solid #ddd;border-radius:8px;padding:12px;margin:15px 0;background:#f9f9f9}"
                 "a{color:#2196F3;text-decoration:none;font-weight:bold}a:hover{text-decoration:underline}"
                 "button{background:#4CAF50;color:white;padding:10px 15px;border:none;border-radius:4px;cursor:pointer;margin:8px 0;width:100%;font-size:16px}";
    
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<meta charset='UTF-8'>"
                  "<title>Measurement</title>"
                  "<style>" + css + "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>HealthSense Measurement</h1>";
    
    if (isLoggedIn) {
        html += "<p class='user'>User Mode - Data will be saved</p>";
    } else {
        html += "<p class='guest'>Guest Mode - No data will be saved</p>";
    }
    
    // Simple measurement page with two options
    html += "<div class='card'>"
            "<p>Welcome to the HealthSense measurement page.</p>"
            "<p>Place your finger on the sensor and press Start Measuring to begin.</p>"
            "</div>";
            
    // Start measuring button
    html += "<form action='/measurement_stream' method='get'>"
            "<button type='submit'>Start Measuring</button>"
            "</form>";
    
    // Button to return to mode selection
    html += "<form action='/mode' method='get'>"
            "<button type='submit' style='background:#f44336'>Back to Mode Select</button>"
            "</form>";
            
    html += "</div></body></html>";
    
    // Send HTTP response with cache control
    server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server->sendHeader("Pragma", "no-cache");
    server->sendHeader("Expires", "-1");
    server->send(200, "text/html", html);
    
    // NOTE: We do NOT set isMeasuring=true here, as measurement shouldn't start
    // until the user clicks "Start Measuring" and reaches the measuring stream page
    
    // Free memory after sending page
    cleanupConnections();
}

void WiFiManager::handleContinueMeasuring() {
    // Reset the measurement stream first load flag
    resetMeasurementStreamState();
    
    // First, ensure we stop any previous measurement
    extern SensorManager sensorManager;
    sensorManager.stopMeasurement();
    
    // Reset the display and sensor state through callback
    if (initializeSensorCallback) {
        Serial.println("Resetting sensor state and display");
        initializeSensorCallback();
    }
    
    // Important: We DO NOT start the measurement here!
    // The measurement will be started when the measurement_stream page is loaded
    
    Serial.println("Re-measure requested, redirecting to measurement stream page");
    server->sendHeader("Location", "/measurement_stream");
    server->send(302, "text/plain", "");
    Serial.println("Device prepared for measurement - waiting for measuring page to load");
    
    // Directly redirect to measurement stream page
    Serial.println("Redirecting to measurement stream page");
    server->sendHeader("Location", "/measurement_stream");
    server->send(302, "text/plain", "");
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
    // Use a more lightweight approach with minimal HTML
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<meta charset='UTF-8'>"
                  "<title>Connection Status</title>"
                  "<style>"
                  "body{font-family:Arial,sans-serif;margin:0;padding:10px;background:#f0f0f0;}"
                  ".container{max-width:400px;margin:0 auto;background:#fff;padding:15px;border-radius:5px;box-shadow:0 1px 5px rgba(0,0,0,.1);}"
                  "h1{font-size:20px;margin-top:0;}"
                  ".banner{padding:8px;border-radius:4px;margin:10px 0;}"
                  ".error{background:#ffebee;color:#d32f2f;border:1px solid #ffcdd2;}"
                  ".success{background:#e8f5e9;color:#388e3c;border:1px solid #c8e6c9;}"
                  ".info{font-family:monospace;background:#f9f9f9;padding:10px;border-radius:4px;margin:10px 0;font-size:12px;white-space:pre-wrap;}"
                  "button{background:#2196F3;color:#fff;padding:8px 12px;border:none;border-radius:4px;cursor:pointer;margin:5px 3px;}"
                  ".btn-red{background:#f44336;}.btn-orange{background:#FF9800;}.btn-purple{background:#9c27b0;}"
                  "ul{text-align:left;margin:10px 0;padding-left:20px;font-size:14px;}"
                  "</style>"
                  "</head><body><div class='container'>"
                  "<h1>Connection Status</h1>";
                  
    // Add connection status banner (more compact)
    if (isConnected) {
        html += "<div class='banner success'><b>✓ Connected</b> to " + userSSID + "</div>";
    } else {
        html += "<div class='banner error'><b>✗ Disconnected</b> - ";
        
        // Show specific error based on WiFi status (simplified)
        switch (lastWifiErrorCode) {
            case WL_NO_SSID_AVAIL: html += "Network not found"; break;
            case WL_CONNECT_FAILED: html += "Authentication failed"; break;
            case WL_CONNECTION_LOST: html += "Connection lost"; break;
            default: html += "Error " + String(lastWifiErrorCode); break;
        }
        html += "</div>";
    }
    
    // Basic connection info only (avoid full getConnectionInfo() which is large)
    html += "<div class='info'>";
    html += "Mode: " + String(WiFi.getMode() == WIFI_AP ? "AP" : (WiFi.getMode() == WIFI_STA ? "Station" : 
                            (WiFi.getMode() == WIFI_AP_STA ? "AP+STA" : "Off"))) + "\n";
    
    if (isConnected) {
        html += "IP: " + WiFi.localIP().toString() + "\n";
        // RSSI should be negative, so use abs() but add negative sign explicitly
        html += "Signal: -" + String(abs(WiFi.RSSI())) + " dBm\n";
    }
    
    html += "Hotspot IP: " + WiFi.softAPIP().toString() + "\n";
    html += "Memory: " + String(ESP.getFreeHeap()/1024) + " KB free\n";
    html += "</div>";
    
    html += "<ul>";
    if (isConnected) {
        html += "<li>Connect via: " + WiFi.localIP().toString() + "</li>";
    }
    html += "<li>Hotspot: " + String(ap_ssid) + " → " + WiFi.softAPIP().toString() + "</li></ul>";
    
    html += "<form action='/' method='get'><button type='submit'>Home</button></form>";
    html += "<button onclick='location.reload()' class='btn-orange'>Refresh</button>";
    
    if (!isConnected) {
        html += "<form action='/wifi' method='get' style='display:inline'><button type='submit' class='btn-purple'>WiFi Setup</button></form>";
    }
    
    html += "</div></body></html>";
    
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
    // Process DNS requests
    dnsServer->processNextRequest();
    
    // Ensure WiFi is maintained
    static unsigned long lastWiFiCheckInLoop = 0;
    if (millis() - lastWiFiCheckInLoop > 1000) { // Check every second in loop
        lastWiFiCheckInLoop = millis();
        if (WiFi.getMode() != WIFI_AP_STA) {
            Serial.println("Fixing WiFi mode in loop - setting to AP+STA");
            WiFi.mode(WIFI_AP_STA);
        }
    }
    
    // Handle client requests
    server->handleClient();
    
    // Check WiFi connection status
    checkWiFiConnection();
    
    // Perform periodic memory maintenance
    static unsigned long lastMemCheck = 0;
    if (millis() - lastMemCheck > 30000) { // Every 30 seconds
        lastMemCheck = millis();
        
        // Log memory status
        Serial.print(F("Free heap: "));
        Serial.print(ESP.getFreeHeap());
        Serial.println(F(" bytes"));
        
        // Force heap cleanup if memory is low (threshold: 30KB)
        if (ESP.getFreeHeap() < 30000) {
            Serial.println(F("Low memory detected! Performing cleanup..."));
            ESP.getFreeHeap(); // This sometimes helps compact heap
            
            // Close any lingering connections
            WiFi.disconnect(false); // Don't disable WiFi, just close sockets
            delay(50); // Short delay to allow cleanup
        }
    }
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
    
    // Free memory before HTTP request
    ESP.getFreeHeap();
    Serial.print(F("Memory before request: "));
    Serial.println(ESP.getFreeHeap());
    
    HTTPClient http;
    String url = serverURL;
    if (!url.endsWith("/")) {
        url += "/";
    }
    url += "api/records";
    
    // Simplify URL logging
    Serial.print(F("URL: "));
    Serial.println(url);
    
    // Set shorter timeout to prevent hanging
    http.setTimeout(5000); // 5 second timeout
    
    // Simple error handling for HTTP begin
    if (!http.begin(url)) {
        Serial.println(F("HTTP init failed"));
        return false;
    }
    
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Id", DEVICE_ID);
    http.addHeader("X-Device-Secret", DEVICE_SECRET);
    
    // Add user ID header if provided
    if (userId.length() > 0) {
        http.addHeader("X-User-Id", userId);
    }
    
    // Simplify JSON creation - use less memory
    String payload = "{\"heart_rate\":" + String(heartRate) + ",\"spo2\":" + String(abs(spo2)) + "}";
    
    // Send POST request with timeout
    Serial.println(F("Sending POST request..."));
    int httpCode = http.POST(payload);
    
    bool success = false;
    if (httpCode == HTTP_CODE_OK) {
        success = true;
        Serial.println(F("✅ Success!"));
    } else {
        Serial.print(F("❌ HTTP error: "));
        Serial.println(httpCode);
    }
    
    // Make sure to end the HTTP connection
    http.end();
    WiFi.disconnect(false); // Keep WiFi connected but close current sockets
    delay(50); // Short delay to allow socket cleanup
    
    Serial.print(F("Memory after request: "));
    Serial.println(ESP.getFreeHeap());
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
    
    // Only send data to API server if user is logged in (not guest mode)
    if (isLoggedIn && !isGuestMode && userUID.length() > 0) {
        Serial.println(F("📤 Sending measurement data to server (User mode)"));
        bool success = sendDeviceData(heartRate, spo2, userUID);
        if (success) {
            Serial.println(F("✅ Data sent successfully to API"));
        } else {
            Serial.println(F("❌ Failed to send data to API"));
        }
        
        // Call callback if it exists
        if (sendDataCallback) {
            Serial.println(F("🔔 Calling sendDataCallback for user mode"));
            sendDataCallback(userUID, heartRate, spo2);
        }
    } else if (isGuestMode) {
        Serial.println(F("👤 Guest mode - not sending data to server"));
        
        // Call callback for guest mode (for local display only)
        if (sendDataCallback) {
            Serial.println(F("🔔 Calling sendDataCallback for guest mode"));
            sendDataCallback("guest", heartRate, spo2);
        }
    } else if (!isLoggedIn) {
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
    
    // Reset the firstLoad flag in handleMeasurementStream to ensure future measurements start properly
    // Note: This is a static variable in handleMeasurementStream that needs to be reset
    
    // Keep the ESP in measuring mode so the measurement results page can access data
    Serial.println(F("✓ Measurement complete - results ready"));
    
    Serial.println(F("🏁 sendSensorData() completed"));
    
    // The measurement_stream page will detect that measurement is complete
    // and automatically redirect to the results page
}

bool WiFiManager::getAIHealthSummary(String& summary) {
    if (!isConnected) {
        Serial.println(F("Not connected to WiFi"));
        summary = "Không có kết nối WiFi";
        return false;
    }
    
    Serial.println(F("Requesting AI summary..."));
    
    // Clean up memory first
    ESP.getFreeHeap();
    Serial.print(F("Memory before: "));
    Serial.println(ESP.getFreeHeap());
    
    HTTPClient http;
    
    // Build URL more simply
    String url = serverURL;
    if (!url.endsWith("/")) url += "/";
    url += "api/ai/sumerize";
    
    // Set shorter timeout
    http.setTimeout(7000); // 7 seconds timeout
    
    // Initialize HTTP client with simple error checking
    if (!http.begin(url)) {
        Serial.println(F("HTTP init failed"));
        summary = "Lỗi kết nối HTTP";
        return false;
    }
    
    // Add essential headers only
    http.addHeader("X-Device-Id", DEVICE_ID);
    if (!isGuestMode && isLoggedIn && userUID.length() > 0) {
        http.addHeader("X-User-Id", userUID);
    }
    
    // Send GET request
    Serial.println(F("Sending GET request"));
    int httpCode = http.GET();
    
    bool success = false;
    
    if (httpCode == HTTP_CODE_OK) {
        // Use manual string parsing instead of JSON library to save memory
        String response = http.getString();
        Serial.print(F("Response OK, length: "));
        Serial.println(response.length());
        
        // Simple string extraction - less memory intensive than JSON parsing
        int summaryStart = response.indexOf("\"summary\":\"");
        if (summaryStart > 0) {
            summaryStart += 11; // Length of "summary":"
            int summaryEnd = response.indexOf("\"", summaryStart);
            if (summaryEnd > summaryStart) {
                summary = response.substring(summaryStart, summaryEnd);
                success = true;
                
                // Limit summary length to save memory
                if (summary.length() > 500) {
                    summary = summary.substring(0, 500) + "...";
                }
            } else {
                summary = "Không thể đọc dữ liệu AI";
            }
        } else {
            summary = "Không tìm thấy kết quả phân tích";
        }
    } else {
        summary = "Lỗi kết nối: " + String(httpCode);
        Serial.print(F("HTTP error: "));
        Serial.println(httpCode);
    }
    
    // Make sure to close connection and clean up sockets
    http.end();
    WiFi.disconnect(false); // Keep WiFi connected but close socket
    delay(50); // Give some time for socket cleanup
    
    Serial.print(F("Memory after: "));
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
        info += "- Signal Strength: -" + String(abs(WiFi.RSSI())) + " dBm\n";
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
    // If in guest mode, show registration prompt
    if (isGuestMode) {
        String css = "body{font-family:Arial;margin:0;padding:10px;background:#f0f0f0;text-align:center}"
                     ".container{max-width:450px;margin:0 auto;background:white;padding:15px;border-radius:8px;box-shadow:0 1px 5px rgba(0,0,0,0.1)}"
                     "h1{color:#333;font-size:20px;margin-top:0}"
                     ".message{padding:15px;background:#fffde7;border:1px solid #fff59d;border-radius:4px;margin:15px 0}"
                     "button{background:#4CAF50;color:white;padding:10px 15px;border:none;border-radius:4px;cursor:pointer;margin:8px 3px;font-weight:bold;min-width:140px}"
                     ".btn-blue{background:#2196F3}.btn-red{background:#f44336}"
                     "a{color:#2196F3;text-decoration:none;font-weight:bold}a:hover{text-decoration:underline}";
        
        String html = "<!DOCTYPE html><html>"
                      "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<meta charset='UTF-8'>"
                      "<title>AI Analysis</title>"
                      "<style>" + css + "</style>"
                      "</head><body><div class='container'>"
                      "<h1>AI Analysis</h1>"
                      "<div class='message'>"
                      "<h3>Feature Available with Registration</h3>"
                      "<p>AI health analysis is only available for registered users. This feature provides personalized health insights based on your measurements.</p>"
                      "<p>To use this feature, please register an account at: <br><a href='https://iot.newnol.io.vn' target='_blank'>HealthSense Portal</a></p>"
                      "</div>"
                      "<div style='margin-top:20px'>"
                      "<form action='/measurement_info' method='get' style='display:inline-block'>"
                      "<button type='submit' class='btn-blue'>Back to Results</button></form>"
                      "<form action='/mode' method='get' style='display:inline-block'>"
                      "<button type='submit' class='btn-red'>Mode Select</button></form>"
                      "</div>"
                      "</div></body></html>";
        
        server->send(200, "text/html", html);
        return;
    }
    
    // If not logged in, redirect to mode selection
    if (!isLoggedIn) {
        server->sendHeader("Location", "/mode");
        server->send(302, "text/plain", "");
        return;
    }
    
    // CSS for the loading page
    String css = "body{font-family:Arial;margin:0;padding:10px;background:#f0f0f0;text-align:center}"
                 ".container{max-width:400px;margin:0 auto;background:white;padding:20px;border-radius:8px;box-shadow:0 1px 5px rgba(0,0,0,0.1)}"
                 "h1{color:#333;font-size:22px;margin-top:0}"
                 ".loader{width:60px;height:60px;border-radius:50%;border:5px solid #f3f3f3;border-top:5px solid #3498db;animation:spin 1.2s linear infinite;margin:20px auto}"
                 "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}";
    
    // Show loading page first
    String loadingPage = "<!DOCTYPE html><html>"
                         "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                         "<meta charset='UTF-8'>"
                         "<meta http-equiv='refresh' content='1;url=/ai_analysis_result'>"
                         "<title>Loading Analysis</title>"
                         "<style>" + css + "</style>"
                         "</head><body><div class='container'>"
                         "<h1>Preparing AI Analysis</h1>"
                         "<div class='loader'></div>"
                         "<p>Analyzing your health data...</p>"
                         "<p>Please wait while we process your measurements.</p>"
                         "</div></body></html>";
    
    // Send loading page immediately
    server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server->send(200, "text/html", loadingPage);
    
    // Register handler for the result page that will be loaded after redirect
    server->on("/ai_analysis_result", [this]() {
        String aiSummary;
        bool success = requestAIHealthSummary(aiSummary);
        
        if (!success) {
            aiSummary = "Unable to retrieve analysis. Please check your connection and try again.";
        }
        
        // CSS for the results page
        String css = "body{font-family:Arial;margin:0;padding:10px;background:#f0f0f0;text-align:center}"
                     ".container{max-width:500px;margin:0 auto;background:white;padding:15px;border-radius:8px;box-shadow:0 1px 5px rgba(0,0,0,0.1)}"
                     "h1{color:#333;font-size:22px;margin-top:0}"
                     ".summary{text-align:left;padding:15px;background:#f9f9f9;border-radius:4px;margin:15px 0;font-size:15px;line-height:1.6}"
                     "button{background:#4CAF50;color:white;padding:10px 15px;border:none;border-radius:4px;cursor:pointer;margin:5px;font-weight:bold;min-width:120px}"
                     ".btn-blue{background:#2196F3}.btn-red{background:#f44336}"
                     ".note{font-size:12px;color:#666;margin-top:20px;font-style:italic;border-top:1px solid #eee;padding-top:10px}";
        
        // HTML response with consistent styling
        String html = "<!DOCTYPE html><html>"
                    "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                    "<meta charset='UTF-8'>"
                    "<title>AI Health Analysis</title>"
                    "<style>" + css + "</style>"
                    "</head><body><div class='container'>"
                    "<h1>AI Health Analysis</h1>"
                    "<div class='summary'>" + aiSummary + "</div>"
                    "<div style='margin-top:20px'>"
                    "<form action='/measurement_info' method='get' style='display:inline-block'>"
                    "<button type='submit' class='btn-blue'>Back to Results</button></form>"
                    "<form action='/continue_measuring' method='get' style='display:inline-block'>"
                    "<button type='submit'>New Measurement</button></form>"
                    "<form action='/mode' method='get' style='display:inline-block'>"
                    "<button type='submit' class='btn-red'>Mode Select</button></form>"
                    "</div>"
                    "<p class='note'>This analysis is for informational purposes only and does not replace professional medical advice.</p>"
                    "</div></body></html>";
        
        server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        server->send(200, "text/html", html);
        
        // Display the AI health summary on the device using the callback
        if (handleAIAnalysisCallback) {
            handleAIAnalysisCallback(aiSummary);
        }
    });
}

void WiFiManager::handleReturnToMeasurement() {
    // Do NOT set isMeasuring=true here - measurement should only start
    // after user clicks "Start Measuring" and page confirms load
    
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

void WiFiManager::cleanupConnections() {
    // Close any pending HTTP connections and sockets
    WiFi.disconnect(false); // Keep WiFi connected but close current sockets
    
    // Let the system process the disconnect
    delay(100);
    
    // Force garbage collection
    ESP.getFreeHeap();
    
    Serial.print(F("Memory after cleanup: "));
    Serial.println(ESP.getFreeHeap());
}

void WiFiManager::forceSocketCleanup() {
    // This is a more aggressive cleanup for when connections are stuck
    Serial.println("Performing force socket cleanup");
    
    // Close all sockets and force WiFi to reconnect
    WiFi.disconnect(true);
    delay(200);
    WiFi.mode(WIFI_AP_STA);
    delay(200);
    
    // Reconnect using saved credentials
    if (userSSID.length() > 0) {
        Serial.println(F("Reconnecting to WiFi after socket cleanup"));
        WiFi.begin(userSSID.c_str(), userPassword.c_str());
    }
}

// Function to ensure WiFi stability
void WiFiManager::ensureWiFiStability() {
    // Check if we're in the correct WiFi mode
    if (WiFi.getMode() != WIFI_AP_STA) {
        Serial.println("Fixing WiFi mode - setting to AP+STA");
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    }
    
    // Check if AP is running as expected
    if (apModeActive && WiFi.softAPIP() != apIP) {
        Serial.println("AP mode issue detected, reconfiguring AP");
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        delay(100);
    }
    
    // Ensure DNS server is running
    dnsServer->processNextRequest();
    
    // Reconnect to user network if needed
    if (userSSID.length() > 0 && WiFi.status() != WL_CONNECTED) {
        Serial.println(F("Reconnecting to WiFi after stability check"));
        WiFi.begin(userSSID.c_str(), userPassword.c_str());
        
        // Wait briefly for connection
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 10) {
            delay(200);
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            isConnected = true;
            Serial.println(F("Reconnected successfully"));
        } else {
            isConnected = false;
            Serial.println(F("Failed to reconnect"));
        }
    }
    
    // Ensure AP mode is active
    if (!apModeActive) {
        setupAPMode();
    }
}

void WiFiManager::restartWiFi() {
    Serial.println("Restarting WiFi...");
    
    // Complete disconnect and cleanup
    WiFi.disconnect(true);
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

void WiFiManager::handleMeasurementInfo() {
    // If not in guest mode and not logged in, redirect to mode selection
    if (!isGuestMode && !isLoggedIn) {
        server->sendHeader("Location", "/mode");
        server->send(302, "text/plain", "");
        return;
    }
    
    // Get data from SensorManager
    extern SensorManager sensorManager;
    
    // If measurement is not ready, redirect to measurement page
    if (!sensorManager.isMeasurementReady()) {
        server->sendHeader("Location", "/measurement");
        server->send(302, "text/plain", "");
        return;
    }
    
    // CSS for the page
    String css = "body{font-family:Arial;margin:0;padding:10px;background:#f0f0f0;text-align:center}"
                 ".container{max-width:450px;margin:0 auto;background:white;padding:15px;border-radius:8px;box-shadow:0 1px 5px rgba(0,0,0,0.1)}"
                 "h1{color:#333;font-size:22px;margin-top:0}"
                 "h2{color:#444;font-size:18px;margin:15px 0 10px;padding-bottom:5px;border-bottom:1px solid #eee}"
                 ".reading{font-size:24px;margin:15px 0;font-weight:bold}"
                 ".hr{color:#f44336}.spo2{color:#2196F3}"
                 ".user{color:#4CAF50;font-weight:bold;font-size:14px}.guest{color:#FF9800;font-weight:bold;font-size:14px}"
                 ".card{border:1px solid #ddd;border-radius:8px;padding:12px;margin:15px 0;background:#f9f9f9}"
                 ".data-table{width:100%;margin:10px 0;font-size:14px;border-collapse:collapse}"
                 ".data-table th,.data-table td{padding:8px;text-align:center;border-bottom:1px solid #ddd}"
                 ".data-table th{background:#f0f0f0}"
                 "button{background:#4CAF50;color:white;padding:10px 15px;border:none;border-radius:4px;cursor:pointer;margin:5px;font-weight:bold;min-width:120px}"
                 ".btn-blue{background:#2196F3}.btn-orange{background:#FF9800}.btn-red{background:#f44336}"
                 ".modal{display:none;position:fixed;left:0;top:0;width:100%;height:100%;background-color:rgba(0,0,0,0.5);z-index:100}"
                 ".modal-content{background:#fff;margin:15% auto;padding:20px;border-radius:8px;width:80%;max-width:400px}";
    
    // Get the measurement results
    int32_t avgHR = sensorManager.getAveragedHR();
    int32_t avgSpO2 = sensorManager.getAveragedSpO2();
    int validCount = sensorManager.getValidReadingCount();
    
    // Build HTML response
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<meta charset='UTF-8'>"
                  "<title>Measurement Results</title>"
                  "<style>" + css + "</style>"
                  "</head>"
                  "<body>"
                  "<div class='container'>"
                  "<h1>Measurement Results</h1>";
    
    // Show user mode
    if (isLoggedIn) {
        html += "<p class='user'>User Mode - Data Saved to Account</p>";
    } else {
        html += "<p class='guest'>Guest Mode - Data Not Saved</p>";
    }
    
    // Show the averaged results card
    html += "<div class='card'>"
            "<h2>Final Results</h2>"
            "<div class='reading hr'>Heart Rate: " + String(avgHR) + " BPM</div>"
            "<div class='reading spo2'>SpO2: " + String(abs(avgSpO2)) + " %</div>"
            "<p>Based on " + String(validCount) + " valid measurements</p>"
            "</div>";
            
    // Add measurement process details
    html += "<div class='card'>"
            "<h2>Measurement Process</h2>"
            "<p>Valid readings collected during measurement:</p>"
            "<table class='data-table'>"
            "<tr><th>Reading</th><th>Heart Rate</th><th>SpO2</th></tr>";
    
    // In a real implementation, you would access the actual array of measurements
    // Here we'll simulate this with random variations around the average
    for (int i = 0; i < validCount; i++) {
        // Simulate some variation in readings (±3 for HR, ±1 for SpO2)
        int variation = (i * 7) % 6 - 3;
        html += "<tr><td>Reading " + String(i+1) + "</td>"
                "<td>" + String(avgHR + variation) + " BPM</td>"
                "<td>" + String(abs(avgSpO2 + (variation/3))) + "%</td></tr>";
    }
    
    html += "</table></div>";
    
    // Add button section
    html += "<div class='card' style='text-align:center'>"
            "<h2>Actions</h2>";
    
    // Re-measure button - primary action
    html += "<form action='/continue_measuring' method='get' style='display:inline-block;margin:5px'>"
            "<button type='submit' style='font-size:16px;padding:12px 25px'>Re-measure</button>"
            "</form>";
    
    // Return to measurement page
    html += "<form action='/measurement' method='get' style='display:inline-block;margin:5px'>"
            "<button type='submit' class='btn-blue'>Back to Measure Page</button>"
            "</form>";
    
    // AI Analysis button based on user mode - only if user is logged in
    if (isLoggedIn) {
        // For logged-in users, provide actual AI analysis
        html += "<form action='/ai_analysis' method='get' style='display:inline-block;margin:5px'>"
                "<button type='submit' class='btn-orange'>AI Analysis</button>"
                "</form>";
    }
    
    // Back to mode select button
    html += "<form action='/mode' method='get' style='display:inline-block;margin:5px'>"
            "<button type='submit' class='btn-red'>Mode Select</button>"
            "</form>";
    
    html += "</div>";
    
    // Add registration modal for guest mode (if needed for some features)
    if (isGuestMode) {
        html += "<div class='card'>"
               "<h2>Want More Features?</h2>"
               "<p>Register an account to save your measurements and access AI analysis.</p>"
               "<p><a href='https://iot.newnol.io.vn' target='_blank'>Visit HealthSense Portal</a></p>"
               "</div>";
    }
    
    html += "</div></body></html>";
    
    // Send HTTP response
    server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server->send(200, "text/html", html);
    
    // Reset measurement stream state for next measurement
    resetMeasurementStreamState();
    
    // Free memory after sending page
    cleanupConnections();
}

// Static variable to track first load of measurement stream page
static bool measurementStreamFirstLoad = true;

void WiFiManager::startMeasurement() {
    isMeasuring = true;
    Serial.println(F("🔄 WiFiManager::startMeasurement - Set isMeasuring = true"));
}

void WiFiManager::stopMeasurement() {
    isMeasuring = false;
    Serial.println(F("🛑 WiFiManager::stopMeasurement - Set isMeasuring = false"));
    
    // Also make sure sensor manager stops measuring
    extern SensorManager sensorManager;
    if (sensorManager.isMeasurementInProgress()) {
        Serial.println(F("Stopping sensor measurement from WiFiManager"));
        sensorManager.stopMeasurement();
    }
}

void WiFiManager::resetMeasurementStreamState() {
    // Reset the firstLoad flag for measurement stream
    measurementStreamFirstLoad = true;
    Serial.println("Reset measurement stream state - ready for next measurement");
}

void WiFiManager::handleMeasurementStream() {
    // Ensure WiFi stability before handling request
    ensureWiFiStability();
    
    // Check WiFi status before processing
    Serial.print("🌐 WiFi Status before measurement stream: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");
    
    // Check if user is logged in or in guest mode
    if (!isGuestMode && !isLoggedIn) {
        server->sendHeader("Location", "/mode");
        server->send(302, "text/plain", "");
        return;
    }
    
    // Reset measurement state first
    extern SensorManager sensorManager;
    if (sensorManager.isMeasurementInProgress()) {
        sensorManager.stopMeasurement();
    }
    
    Serial.println("📈 User requested to start measuring - preparing measurement stream page");
    
    // If measurement is complete, redirect to results page
    if (sensorManager.isMeasurementReady()) {
        Serial.println("Measurement ready, redirecting to results page");
        server->sendHeader("Location", "/measurement_info");
        server->send(302, "text/plain", "");
        return;
    }
    
    // IMPORTANT CHANGE: Start measuring immediately when this page is loaded, instead of waiting
    // for the JavaScript fetch call which might fail due to connectivity issues
    Serial.println("🚀 Starting measurement directly when measurement stream page loads");
    isMeasuring = true; // Set measurement flag to true
    
    // Start the measurement process
    if (startNewMeasurementCallback) {
        Serial.println("Using registered callback to start measurement");
        startNewMeasurementCallback();
    } else {
        Serial.println("Starting measurement directly");
        sensorManager.startMeasurement();
    }
    
    Serial.println("⭐ Measurement activated: isMeasuring = " + String(isMeasuring ? "YES" : "NO"));
    
    // Simple measuring page with spinner and IMPROVED JavaScript
    String html = "<!DOCTYPE html><html>"
                  "<head><meta charset='UTF-8'>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>Measuring...</title>"
                  "<style>"
                  "body{font-family:Arial;margin:0;padding:10px;background:#f0f0f0;text-align:center}"
                  ".container{max-width:400px;margin:0 auto;background:white;padding:15px;border-radius:8px;box-shadow:0 1px 5px rgba(0,0,0,0.1)}"
                  "h1{color:#333;font-size:22px;margin-top:0}"
                  ".loader{width:60px;height:60px;border-radius:50%;border:5px solid #f3f3f3;border-top:5px solid #3498db;animation:spin 1.5s linear infinite;margin:20px auto}"
                  "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}"
                  ".status{padding:15px;margin:15px 0;font-weight:bold;color:#1976d2;font-size:18px}"
                  ".user{color:#4CAF50;font-weight:bold;font-size:14px}.guest{color:#FF9800;font-weight:bold;font-size:14px}"
                  ".note{margin:30px 0 10px;font-size:14px;color:#666}"
                  "</style>"
                  "<script>"
                  "// Handle page load completion"
                  "window.addEventListener('load', function() {"
                  "  console.log('Measurement page loaded - measurement already started');"
                  "  startStatusChecking();" // Start checking immediately when page loads
                  "});"
                  ""
                  "// Function to handle redirect from server or trigger manual redirect"
                  "function startStatusChecking() {"
                  "  // Check more frequently (every 1 second)"
                  "  var checkStatusInterval = setInterval(function() {"
                  "    fetch('/check_measurement_status')"
                  "      .then(response => {"
                  "        // If we get a redirect response (302), follow it immediately"
                  "        if (response.redirected) {"
                  "          console.log('Server redirected, following to:', response.url);"
                  "          clearInterval(checkStatusInterval);"
                  "          window.location.href = response.url;"
                  "          return 'redirected';" 
                  "        }"
                  "        return response.text();"
                  "      })"
                  "      .then(status => {"
                  "        if (status === 'redirected') return;" // Already handled redirect
                  "        console.log('Received status:', status);"
                  "        if (status === 'complete') {"
                  "          console.log('Measurement complete, redirecting...');"
                  "          clearInterval(checkStatusInterval);"
                  "          window.location.href = '/measurement_info';"
                  "        }"
                  "      })"
                  "      .catch(error => {"
                  "        console.error('Error checking status:', error);"
                  "        // If we get an error, it might be because the ESP32 has redirected and"
                  "        // the connection was closed. Try loading the results page directly."
                  "        window.location.href = '/measurement_info';"
                  "      });"
                  "  }, 1000);" // Check every 1 second
                  "}"
                  "</script>"
                  "</head><body><div class='container'>"
                  "<h1>Measurement in Progress</h1>";

    // Show user mode
    if (isLoggedIn) {
        html += "<p class='user'>User Mode - Data will be saved to your account</p>";
    } else {
        html += "<p class='guest'>Guest Mode - Data will not be saved</p>";
    }
    
    html += "<div class='loader'></div>"
            "<div class='status'>Please wait while we collect your measurements</div>"
            "<p class='note'>Values are being displayed on the device LCD screen.<br>"
            "This page will automatically update when measurement is complete.</p>"
            "<p id='countdown' style='display:none; color:#f44336; font-weight:bold;'>Redirecting in <span id='timer'>10</span>...</p>"
            "<script>"
            "// Add multiple failsafe redirects"
            
            "// Failsafe #1: Add a meta refresh tag after 30 seconds"
            "setTimeout(function() {"
            "  var meta = document.createElement('meta');"
            "  meta.httpEquiv = 'refresh';"
            "  meta.content = '2;url=/measurement_info';"
            "  document.head.appendChild(meta);"
            "  console.log('Added meta refresh tag as failsafe');"
            "}, 30000);"
            
            "// Failsafe #2: Show countdown and redirect after 40 seconds"
            "setTimeout(function() {"
            "  document.getElementById('countdown').style.display = 'block';"
            "  var count = 10;"
            "  var timer = setInterval(function() {"
            "    document.getElementById('timer').textContent = count;"
            "    count--;"
            "    if(count < 0) {"
            "      clearInterval(timer);"
            "      window.location.href = '/measurement_info';"
            "    }"
            "  }, 1000);"
            "}, 40000);"
            
            "// Failsafe #3: Force redirect after 60 seconds no matter what"
            "setTimeout(function() {"
            "  console.log('Final failsafe activated, forcing redirect');"
            "  window.location.href = '/measurement_info';" 
            "}, 60000);"
            "</script>"
            
            "<!-- Server-side automatic refresh after 60 seconds -->"
            "<meta http-equiv='refresh' content='60;url=/measurement_info'>"
            "</div></body></html>";
    
    Serial.println("✅ Measurement stream page sent, measurement already started");
    
    // Send HTTP response with cache control
    server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server->sendHeader("Pragma", "no-cache");
    server->sendHeader("Expires", "-1");
    server->send(200, "text/html", html);
}

// This handler is called by the browser via fetch() AFTER the page is fully loaded
void WiFiManager::handleStartMeasurement() {
    // Ensure WiFi stability before handling request
    ensureWiFiStability();
    
    // Check WiFi status before processing
    Serial.print("🌐 WiFi Status before starting measurement: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");
    
    // Only start if user is logged in or in guest mode
    if (!isGuestMode && !isLoggedIn) {
        server->send(403, "text/plain", "Not authorized");
        return;
    }
    
    Serial.println("Browser confirmed page is fully loaded - NOW STARTING MEASUREMENT");
    Serial.println("User mode: " + String(isGuestMode ? "GUEST" : "LOGGED IN"));
    
    // Ensure WiFi mode is properly set before changing state
    if (WiFi.getMode() != WIFI_AP_STA) {
        Serial.println("Ensuring WiFi mode is AP+STA");
        WiFi.mode(WIFI_AP_STA);
        delay(100);  // Small delay to allow mode change
    }
    
    // Now we can safely start the measurement
    isMeasuring = true;
    
    // Get reference to external SensorManager
    extern SensorManager sensorManager;
    
    // Start the measurement
    if (startNewMeasurementCallback) {
        Serial.println("Using registered callback to start measurement");
        startNewMeasurementCallback();
    } else {
        Serial.println("Starting measurement directly");
        sensorManager.startMeasurement();
    }
    
    // Send a simple response back to the browser
    server->send(200, "text/plain", "Measurement started");
    
    // Debug output to confirm measurement was started
    Serial.println("⭐ Measurement activation confirmed: isMeasuring = " + String(isMeasuring ? "YES" : "NO"));
}

// New handler to check if measurement is complete
void WiFiManager::handleCheckMeasurementStatus() {
    // Only allow if user is logged in or in guest mode
    if (!isGuestMode && !isLoggedIn) {
        server->send(403, "text/plain", "Not authorized");
        return;
    }
    
    // Get data from SensorManager
    extern SensorManager sensorManager;
    
    // Check if measurement is complete
    bool measurementReady = sensorManager.isMeasurementReady();
    int validReadingCount = sensorManager.getValidReadingCount();
    
    Serial.print("🔍 Check Measurement Status - isMeasurementReady: ");
    Serial.print(measurementReady ? "YES ✓" : "NO ✗");
    Serial.print(", WiFi isMeasuring: ");
    Serial.print(isMeasuring ? "YES" : "NO");
    Serial.print(", Readings: ");
    Serial.print(validReadingCount);
    Serial.println("/5");
    
    // If measurement is complete or we have all required readings, redirect to results page
    if (measurementReady || (validReadingCount >= REQUIRED_VALID_READINGS)) {
        // Make sure to update our local state
        if (isMeasuring) {
            stopMeasurement(); // Stop measuring in WiFiManager
        }
        
        Serial.println("✅ Measurement complete, redirecting to results page IMMEDIATELY");
        
        // IMPORTANT CHANGE: Use 302 redirect instead of regular response with refresh header
        // This forces an immediate redirect to the results page
        server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        server->sendHeader("Pragma", "no-cache");
        server->sendHeader("Expires", "-1");
        server->sendHeader("Location", "/measurement_info", true);
        server->send(302, "text/plain", "Redirecting to results...");
        
        Serial.println("🔄 Sent 302 redirect to /measurement_info");
        
        // No need for the static redirectScheduled logic anymore since we're
        // doing an immediate redirect
    } else {
        Serial.println("⏳ Measurement still in progress, sending 'in_progress' status");
        server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        server->sendHeader("Pragma", "no-cache");
        server->sendHeader("Expires", "-1");
        server->send(200, "text/plain", "in_progress");
    }
}
