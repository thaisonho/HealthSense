#include "display_manager.h"
#include "images.h"

DisplayManager::DisplayManager(Adafruit_ST7735* tft, const uint16_t* logo, uint16_t logoWidth, uint16_t logoHeight) :
    tft(tft),
    logo(logo),
    logoWidth(logoWidth),
    logoHeight(logoHeight) {
}

void DisplayManager::begin() {
    tft->initR(INITR_GREENTAB); // Initialize with the correct display type
    tft->setRotation(1);        // Set display rotation (0=Portrait, 1=Landscape)
    tft->fillScreen(ST7735_BLACK); // Clear the screen with black color
    tft->setTextSize(1);
    
    // Draw the logo image on the display (top portion)
    tft->drawRGBBitmap((160 - logoWidth)/2, 0, logo, logoWidth, logoHeight);
}

void DisplayManager::showWiFiSetupScreen(const char* apSSID, const char* apPassword) {
    tft->fillRect(0, 70, 160, 90, ST7735_BLACK);
    tft->setTextColor(ST7735_RED);
    tft->setCursor(5, 70);
    tft->println("Not connected to Internet");
    tft->setCursor(5, 80);
    tft->setTextColor(ST7735_YELLOW);
    tft->println("Please connect to WiFi:");
    tft->setCursor(5, 90);
    tft->print("SSID: ");
    tft->println(apSSID);
    tft->setCursor(5, 100);
    tft->print("Password: ");
    tft->println(apPassword);
    tft->setCursor(5, 110);
    tft->setTextColor(ST7735_WHITE);
    tft->println("Then open in browser:");
    tft->setCursor(5, 120);
    tft->println("http://192.168.4.1");
}

void DisplayManager::showConnectionAttempt(String ssid) {
    tft->fillRect(0, 130, 160, 30, ST7735_BLACK);
    tft->setCursor(5, 130);
    tft->setTextColor(ST7735_YELLOW);
    tft->print("Connecting to ");
    tft->println(ssid);
}

void DisplayManager::showConnectionSuccess(String ipAddress) {
    tft->fillRect(0, 130, 160, 30, ST7735_BLACK);
    tft->setCursor(5, 130);
    tft->setTextColor(ST7735_GREEN);
    tft->println("WiFi Connected");
    tft->setCursor(5, 140);
    tft->println(ipAddress);
}

void DisplayManager::showConnectionFailure() {
    tft->fillRect(0, 130, 160, 30, ST7735_BLACK);
    tft->setCursor(5, 130);
    tft->setTextColor(ST7735_RED);
    tft->println("WiFi Connection Failed");
}

void DisplayManager::showGuestMode() {
    tft->fillRect(0, 150, 160, 30, ST7735_BLACK);
    tft->setCursor(5, 150);
    tft->setTextColor(ST7735_CYAN);
    tft->println("GUEST MODE");
    
    // Show registration link info
    tft->setCursor(5, 160);
    tft->setTextColor(ST7735_YELLOW);
    tft->println("Register: localhost:30001");
    
    // Show WiFi reconfiguration option
    showWiFiReconfigOption();
}

void DisplayManager::showLoggedIn() {
    tft->fillRect(0, 150, 160, 30, ST7735_BLACK);
    tft->setCursor(5, 150);
    tft->setTextColor(ST7735_GREEN);
    tft->println("USER LOGGED IN");
    
    // Show WiFi reconfiguration option
    showWiFiReconfigOption();
}

void DisplayManager::showLoginPage() {
    tft->fillRect(0, 70, 160, 90, ST7735_BLACK);
    tft->setTextColor(ST7735_WHITE);
    tft->setCursor(5, 70);
    tft->println("Login Required");
    tft->setCursor(5, 90);
    tft->println("Please visit:");
    tft->setCursor(5, 100);
    tft->setTextColor(ST7735_YELLOW);
    tft->print("http://");
    tft->println(WiFi.localIP().toString());
    tft->setCursor(5, 110);
    tft->setTextColor(ST7735_WHITE);
    tft->println("to login with your");
    tft->setCursor(5, 120);
    tft->println("email and password");
}

void DisplayManager::showLoginStatus(bool success) {
    tft->fillRect(0, 130, 160, 20, ST7735_BLACK);
    tft->setCursor(5, 130);
    
    if (success) {
        tft->setTextColor(ST7735_GREEN);
        tft->println("Login successful!");
        tft->setCursor(5, 140);
        tft->println("Measuring will begin...");
    } else {
        tft->setTextColor(ST7735_RED);
        tft->println("Login failed!");
        tft->setCursor(5, 140);
        tft->println("Please try again");
    }
}

void DisplayManager::showWiFiReconfigOption() {
    tft->setCursor(5, 170);
    tft->setTextColor(ST7735_WHITE);
    tft->print("Reconfigure WiFi: ");
    tft->println(WiFi.localIP().toString());
}

void DisplayManager::setupSensorUI() {
    tft->fillRect(0, 70, 160, 50, ST7735_BLACK);
    tft->setTextColor(ST7735_WHITE);
    tft->setCursor(5, 70);
    tft->println("HealthSense Monitor");
    
    tft->setCursor(5, 90);
    tft->setTextColor(ST7735_RED);
    tft->print("Heart Rate: ");
    tft->println("-- BPM");
    
    tft->setCursor(5, 110);
    tft->setTextColor(ST7735_BLUE);
    tft->print("SpO2: ");
    tft->println("-- %");
    
    // Add WiFi reconfiguration info
    tft->setTextSize(1);
    tft->setCursor(5, 130);
    tft->setTextColor(ST7735_YELLOW);
    tft->println("IP: " + WiFi.localIP().toString());
    tft->setCursor(5, 140);
    tft->println("Visit to reconfigure WiFi");
}

void DisplayManager::updateSensorReadings(int32_t heartRate, bool validHR, int32_t spo2, bool validSPO2, MeasurementPhase phase) {
    // Clear previous readings
    tft->fillRect(75, 90, 80, 10, ST7735_BLACK);  // Clear heart rate value area
    tft->fillRect(45, 110, 80, 10, ST7735_BLACK); // Clear SpO2 value area
    
    // Display heart rate
    tft->setCursor(75, 90);
    tft->setTextColor(ST7735_RED);
    
    // Only show valid readings in RELIABLE phase
    if (validHR && phase == PHASE_RELIABLE) {
        tft->print(heartRate);
        tft->print(" BPM");
    } else {
        tft->print("-- BPM");
    }
    
    // Display SpO2
    tft->setCursor(45, 110);
    tft->setTextColor(ST7735_BLUE);
    if (validSPO2 && phase == PHASE_RELIABLE) {
        tft->print(spo2);
        tft->print(" %");
    } else {
        tft->print("-- %");
    }
    
    // Update the measurement status based on the phase
    showMeasuringStatus(phase);
}

void DisplayManager::showMeasuringStatus(MeasurementPhase phase) {
    tft->fillRect(5, 130, 160, 10, ST7735_BLACK);
    tft->setCursor(5, 130);
    
    switch (phase) {
        case PHASE_INIT:
            // Initial phase - warming up (first 1-3 seconds)
            tft->setTextColor(ST7735_YELLOW);
            tft->print("Warming up...");
            break;
            
        case PHASE_STABILIZE:
            // Stabilization phase (next 3-8 seconds)
            tft->setTextColor(ST7735_CYAN);
            tft->print("Stabilizing signal...");
            break;
            
        case PHASE_RELIABLE:
            // Reliable results phase (after 5-10 seconds)
            tft->setTextColor(ST7735_GREEN);
            tft->print("Reliable reading");
            break;
            
        default:
            tft->setTextColor(ST7735_GREEN);
            tft->print("Measuring...");
            break;
    }
}

void DisplayManager::showFingerStatus(bool fingerDetected) {
    tft->fillRect(5, 130, 160, 10, ST7735_BLACK);
    tft->setCursor(5, 130);
    tft->setTextColor(ST7735_GREEN);
    
    if (fingerDetected) {
        tft->print("Finger detected");
    } else {
        tft->print("Place finger...");
    }
}
