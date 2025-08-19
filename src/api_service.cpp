#include "api_service.h"

APIService::APIService(const String& baseUrl) :
    serverBaseUrl(baseUrl),
    uid(""),
    isAuthenticated(false) {
}

bool APIService::authenticateUser(const String& username, const String& password) {
    if (username.isEmpty() || password.isEmpty()) {
        return false;
    }

    HTTPClient http;
    http.begin(serverBaseUrl + "/auth/login");
    http.addHeader("Content-Type", "application/json");
    
    // Create JSON payload
    String jsonPayload = "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";
    
    // Send POST request
    int httpResponseCode = http.POST(jsonPayload);
    
    // Process response
    if (httpResponseCode == 200) {
        String response = http.getString();
        
        // Manually parse JSON response to extract UID
        // This approach avoids using ArduinoJson library for simpler implementation
        int uidStart = response.indexOf("\"uid\":\"") + 7;
        int uidEnd = response.indexOf("\"", uidStart);
        
        if (uidStart > 7 && uidEnd > uidStart) {
            uid = response.substring(uidStart, uidEnd);
            isAuthenticated = true;
            return true;
        }
    }
    
    // Authentication failed
    isAuthenticated = false;
    uid = "";
    return false;
}

bool APIService::sendHealthData(int heartRate, int spo2) {
    // Only send data if authenticated
    if (!isAuthenticated || uid.isEmpty()) {
        return false;
    }
    
    HTTPClient http;
    http.begin(serverBaseUrl + "/health/data");
    http.addHeader("Content-Type", "application/json");
    
    // Create JSON payload
    String jsonPayload = "{\"uid\":\"" + uid + "\",\"heartrate\":" + String(heartRate) + ",\"spo2\":" + String(spo2) + "}";
    
    // Send POST request
    int httpResponseCode = http.POST(jsonPayload);
    
    // Check if successful
    return (httpResponseCode >= 200 && httpResponseCode < 300);
}
