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
    tft->fillRect(0, 150, 160, 10, ST7735_BLACK);
    tft->setCursor(5, 150);
    tft->setTextColor(ST7735_CYAN);
    tft->println("GUEST MODE");
}

void DisplayManager::showLoggedIn() {
    tft->fillRect(0, 150, 160, 10, ST7735_BLACK);
    tft->setCursor(5, 150);
    tft->setTextColor(ST7735_WHITE);
    tft->println("USER LOGGED IN");
}

void DisplayManager::showAuthenticated(String uid) {
    tft->fillRect(0, 150, 160, 20, ST7735_BLACK);
    tft->setCursor(5, 150);
    tft->setTextColor(ST7735_GREEN);
    tft->println("AUTHENTICATED");
    tft->setCursor(5, 160);
    tft->print("UID: ");
    tft->println(uid);
}

void DisplayManager::showAuthenticationFailed() {
    tft->fillRect(0, 150, 160, 10, ST7735_BLACK);
    tft->setCursor(5, 150);
    tft->setTextColor(ST7735_RED);
    tft->println("AUTH FAILED");
}

void DisplayManager::showDataTransmitted(bool success) {
    tft->fillRect(0, 140, 160, 10, ST7735_BLACK);
    tft->setCursor(5, 140);
    if (success) {
        tft->setTextColor(ST7735_GREEN);
        tft->println("Data sent");
    } else {
        tft->setTextColor(ST7735_RED);
        tft->println("Send failed");
    }
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
}

void DisplayManager::updateSensorReadings(int32_t heartRate, bool validHR, int32_t spo2, bool validSPO2) {
    // Clear previous readings
    tft->fillRect(75, 90, 80, 10, ST7735_BLACK);  // Clear heart rate value area
    tft->fillRect(45, 110, 80, 10, ST7735_BLACK); // Clear SpO2 value area
    
    // Display heart rate
    tft->setCursor(75, 90);
    tft->setTextColor(ST7735_RED);
    if (validHR) {
        tft->print(heartRate);
        tft->print(" BPM");
    } else {
        tft->print("-- BPM");
    }
    
    // Display SpO2
    tft->setCursor(45, 110);
    tft->setTextColor(ST7735_BLUE);
    if (validSPO2) {
        tft->print(spo2);
        tft->print(" %");
    } else {
        tft->print("-- %");
    }
}

void DisplayManager::showMeasuringStatus() {
    tft->fillRect(5, 130, 160, 10, ST7735_BLACK);
    tft->setCursor(5, 130);
    tft->setTextColor(ST7735_GREEN);
    tft->print("Measuring...");
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
