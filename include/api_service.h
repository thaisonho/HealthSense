#ifndef API_SERVICE_H
#define API_SERVICE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class APIService {
private:
    String serverBaseUrl;
    String uid;
    bool isAuthenticated;

public:
    APIService(const String& baseUrl = "http://localhost:30000/api");
    
    // User authentication
    bool authenticateUser(const String& username, const String& password);
    
    // Send health data
    bool sendHealthData(int heartRate, int spo2);
    
    // Getters
    bool isUserAuthenticated() const { return isAuthenticated; }
    String getUserId() const { return uid; }
    
    // Setters
    void setServerBaseUrl(const String& baseUrl) { serverBaseUrl = baseUrl; }
};

#endif // API_SERVICE_H
