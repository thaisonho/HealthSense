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
    
    // Show the AI Analysis button
    showAIAnalysisButton();
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

void DisplayManager::showAIAnalysisButton() {
    // Draw the AI Analysis button
    tft->fillRoundRect(20, 150, 120, 20, 5, ST7735_BLUE);
    tft->setTextColor(ST7735_WHITE);
    tft->setTextSize(1);
    tft->setCursor(45, 156);
    tft->print("AI ANALYSIS");
}

void DisplayManager::showAIAnalysisLoading() {
    // Clear the button area and show loading indicator
    tft->fillRoundRect(20, 150, 120, 20, 5, ST7735_MAGENTA);
    tft->setTextColor(ST7735_WHITE);
    tft->setTextSize(1);
    tft->setCursor(40, 156);
    tft->print("ANALYZING...");
}

void DisplayManager::displayAIHealthSummary(const String& summary) {
    // Clear the entire screen for full-screen display
    tft->fillScreen(ST7735_BLACK);
    
    // Draw a nice header with gradient
    for (int16_t i = 0; i < 15; i++) {
        uint16_t color = tft->color565(0, 64 + i * 12, 128 + i * 8);
        tft->drawFastHLine(0, i, 160, color);
    }
    
    // Draw header text
    tft->setTextSize(1);
    tft->setTextColor(ST7735_WHITE);
    tft->setCursor(10, 5);
    tft->print("AI HEALTH SUMMARY");
    
    // Draw a separator line
    tft->drawFastHLine(0, 16, 160, ST7735_CYAN);
    
    // Display the summary text with word wrapping
    tft->setTextColor(ST7735_GREEN);
    
    // Word wrap and display the summary
    int16_t xPos = 5;
    int16_t yPos = 25;
    String currentWord = "";
    
    for (uint16_t i = 0; i < summary.length(); i++) {
        char c = summary.charAt(i);
        
        if (c == ' ' || c == '\n' || i == summary.length() - 1) {
            if (i == summary.length() - 1 && c != ' ' && c != '\n') {
                currentWord += c;
            }
            
            int16_t wordWidth = currentWord.length() * 6; // Approximate width calculation
            
            if (xPos + wordWidth > 155) {
                xPos = 5;
                yPos += 10; // Line height
            }
            
            tft->setCursor(xPos, yPos);
            tft->print(currentWord);
            
            xPos += wordWidth + 3; // Space width
            currentWord = "";
            
            if (c == '\n') {
                xPos = 5;
                yPos += 10;
            }
        } else {
            currentWord += c;
        }
    }
    
    // Add a footer note about returning via web interface
    tft->setTextColor(ST7735_YELLOW);
    tft->setCursor(5, 140);
    tft->print("Use web interface to return");
}
