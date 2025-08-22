# HealthSense Developer Guide

This guide provides detailed information about the code structure, implementation details, and customization options for developers working with the HealthSense project.

## Project Architecture

HealthSense follows a modular architecture with three main components:

1. **WiFiManager**: Handles network connectivity, web server, and user interfaces
2. **SensorManager**: Controls the MAX30105 sensor and processes health measurements
3. **DisplayManager**: Manages the TFT display and UI elements

These components communicate through callbacks and a simple state machine in the main application.

## Directory Structure

```
/HealthSense
│
├── src/                  # Source files (.cpp)
│   ├── main.cpp          # Entry point and application logic
│   ├── wifi_manager.cpp  # WiFi and web server implementation
│   ├── sensor_manager.cpp # MAX30105 sensor control
│   ├── display_manager.cpp # TFT display control
│   ├── images.cpp        # Image data for display
│   └── utils.cpp         # Utility functions
│
├── include/              # Header files (.h)
│   ├── wifi_manager.h    # WiFi and server declarations
│   ├── sensor_manager.h  # Sensor handling declarations
│   ├── display_manager.h # Display interface declarations
│   ├── esp32_max30105_fix.h # MAX30105 library fix for ESP32
│   ├── common_types.h    # Shared data types and constants
│   └── images.h          # Image data declarations
│
├── lib/                  # External libraries
│
└── platformio.ini        # Project configuration
```

## Core Components

### WiFiManager

The WiFiManager class handles:
- WiFi connection setup (AP mode and client mode)
- Web server configuration and routing
- User authentication and session management
- HTML page generation for the web interface
- Measurement initiation and status reporting
- API communication with the backend server

**Key Files:**
- `include/wifi_manager.h`: Class declaration
- `src/wifi_manager.cpp`: Implementation

**Key Methods:**
- `begin()`: Initializes WiFi and web server
- `loop()`: Processes incoming connections
- `handleRoot()`: Root web page handler
- `handleMeasurementStream()`: Measurement interface
- `handleCheckMeasurementStatus()`: Status checking endpoint
- `sendSensorData()`: Sends data to server API

### SensorManager

The SensorManager class handles:
- MAX30105 sensor initialization and configuration
- Heart rate and SpO2 measurement
- Signal processing and data validation
- Reading averaging and quality control
- Measurement state management

**Key Files:**
- `include/sensor_manager.h`: Class declaration
- `src/sensor_manager.cpp`: Implementation

**Key Methods:**
- `begin()`: Initializes sensor with I2C pins
- `initializeSensor()`: Configures sensor parameters
- `processReadings()`: Processes sensor data
- `startMeasurement()`: Begins measurement sequence
- `isFingerDetected()`: Detects finger presence

### DisplayManager

The DisplayManager class handles:
- TFT display initialization
- UI rendering and updates
- Measurement display formatting
- Status indicators and icons

**Key Files:**
- `include/display_manager.h`: Class declaration
- `src/display_manager.cpp`: Implementation

**Key Methods:**
- `begin()`: Initializes display
- `showWelcomeScreen()`: Displays splash screen
- `updateSensorReadings()`: Updates displayed measurements
- `showFingerStatus()`: Updates finger detection indicator
- `showWiFiStatus()`: Updates network status indicator

## Main Application Logic

The `main.cpp` file serves as the entry point and contains:
- Component initialization
- Callback registration between components
- Main loop execution
- State machine for application flow

**Key Functions:**
- `setup()`: Initializes all components
- `loop()`: Main execution loop
- `initializeSensor()`: Callback for sensor initialization
- `updateConnectionStatus()`: Callback for WiFi status changes
- `sendSensorData()`: Callback for sending measurement data

## Flow Implementations

### WiFi Connection Flow

1. The ESP32 starts in AP mode with SSID "HealthSense"
2. User connects to this network and visits the captive portal
3. User enters credentials for their WiFi network
4. `WiFiManager::handleConnect()` attempts to connect to the user's network
5. On success, the device operates in dual mode (AP+STA)
6. On failure, it remains in AP mode only

### Measurement Flow

1. User selects "Start Measuring" on the web interface
2. `WiFiManager::handleMeasurementStream()` is called, showing the measurement page
3. `WiFiManager::startMeasurement()` sets the measuring flag
4. `SensorManager::startMeasurement()` begins collecting readings
5. JavaScript on the client periodically calls `handleCheckMeasurementStatus()`
6. When enough valid readings are collected, `measurementComplete` flag is set
7. Browser is redirected to the results page
8. `WiFiManager::handleMeasurementInfo()` displays the averaged results

## Key Implementation Details

### Measurement Process

The measurement implementation collects multiple valid readings to ensure accuracy:

```cpp
// In SensorManager::processReadings()
if (validHeartRate && validSPO2 && isFingerDetected()) {
    validReadings[validReadingCount][0] = heartRate;
    validReadings[validReadingCount][1] = spo2;
    validReadingCount++;
    
    // Check if we have enough valid readings
    if (validReadingCount >= REQUIRED_VALID_READINGS) {
        // Calculate averages
        int32_t totalHR = 0;
        int32_t totalSpO2 = 0;
        
        for (int i = 0; i < REQUIRED_VALID_READINGS; i++) {
            totalHR += validReadings[i][0];
            totalSpO2 += validReadings[i][1];
        }
        
        averagedHR = totalHR / REQUIRED_VALID_READINGS;
        averagedSpO2 = totalSpO2 / REQUIRED_VALID_READINGS;
        measurementComplete = true;
        isMeasuring = false;
        
        // Call measurement complete callback
        if (measurementCompleteCallback) {
            measurementCompleteCallback(averagedHR, averagedSpO2);
        }
    }
}
```

### Browser Redirection

The system implements multiple redundant methods for redirecting the browser to the results page:

```cpp
// In WiFiManager::handleCheckMeasurementStatus()
if (measurementReady || (validReadingCount >= REQUIRED_VALID_READINGS)) {
    // Make sure to update our local state
    if (isMeasuring) {
        stopMeasurement(); // Stop measuring in WiFiManager
    }
    
    // IMPORTANT: Use 302 redirect instead of regular response
    server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server->sendHeader("Location", "/measurement_info", true);
    server->send(302, "text/plain", "Redirecting to results...");
}
```

### Finger Detection Algorithm

The system uses a sophisticated algorithm to detect finger presence:

```cpp
bool SensorManager::isFingerDetected() const {
    // Check if sensor is ready
    if (!sensorReady) {
        return false;
    }
    
    // Set appropriate thresholds
    uint32_t threshold_ir = IR_SIGNAL_THRESHOLD;
    uint32_t threshold_red = RED_SIGNAL_THRESHOLD;
    
    // Calculate average values
    uint32_t avgIR = 0;
    uint32_t avgRed = 0;
    int validSamples = 0;
    
    // Calculate the averages from samples
    // ...code omitted for brevity...
    
    // Check if IR signal is in the expected range for a finger
    bool signalPresent = (avgIR > threshold_ir) && (avgRed > threshold_red);
    // For MAX30105, IR should be greater than RED but not by too much
    bool properRatio = (avgIR > avgRed * 0.9) && (avgIR < avgRed * 1.5);
    
    return signalPresent && properRatio;
}
```

## Customization Guide

### Adding New Web Pages

To add a new page to the web interface:

1. Create a new handler method in the WiFiManager class:

```cpp
void WiFiManager::handleNewPage() {
    String html = "<!DOCTYPE html><html>"
                  "<head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>New Page</title>"
                  "<style>" + commonCSS + "</style>"
                  "</head><body><div class='container'>"
                  "<h1>Your New Page</h1>"
                  "<p>Content goes here...</p>"
                  "</div></body></html>";
    
    server->send(200, "text/html", html);
}
```

2. Register the handler in the `WiFiManager::begin()` method:

```cpp
server->on("/new-page", [this](){ this->handleNewPage(); });
```

### Modifying the Measurement Process

To adjust the measurement process:

1. Change the number of readings required by modifying `REQUIRED_VALID_READINGS` in `sensor_manager.h`
2. Adjust validation thresholds by modifying constants like `MIN_VALID_HR`, `MAX_VALID_HR`, etc.
3. Modify timeout duration by changing `MEASUREMENT_TIMEOUT_MS`

### Adding a New Sensor

To integrate a new sensor:

1. Create a new manager class for your sensor
2. Initialize it in `main.cpp`
3. Set up callbacks for data exchange
4. Update the DisplayManager to show the new measurements

## API Integration

The HealthSense device integrates with a backend API for user authentication and data storage:

### Authentication Endpoint

```cpp
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
    
    // Process response
    // ...code omitted for brevity...
}
```

### Measurement Data Endpoint

```cpp
bool WiFiManager::sendDeviceData(int32_t heartRate, int32_t spo2, String userId) {
    if (!isConnected) {
        return false;
    }
    
    HTTPClient http;
    String url = serverURL;
    if (!url.endsWith("/")) url += "/";
    url += "api/records";
    
    http.setTimeout(5000); // 5 second timeout
    
    if (!http.begin(url)) {
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
    String payload = "{\"heart_rate\":" + String(heartRate) + ",\"spo2\":" + String(spo2) + "}";
    
    // Send POST request with timeout
    int httpCode = http.POST(payload);
    
    // Process response
    // ...code omitted for brevity...
}
```

## Advanced Topics

### Memory Management

The ESP32 has limited memory, so the code includes:

1. Buffer size optimization
2. String concatenation minimization
3. Memory cleanup with `cleanupConnections()`
4. Connection pooling to prevent leaks
5. Periodic heap checks

### Error Handling

The system implements various error handling mechanisms:

1. WiFi connection retry logic
2. I2C sensor detection and reset
3. Measurement timeout handling
4. API request error handling
5. HTTP response validation

### State Management

The application uses a state machine approach:

```cpp
// State machine for app behavior
switch (currentState) {
  case STATE_SETUP:
    // Wait for user to select connection mode
    break;
    
  case STATE_CONNECTING:
    // Wait for connection to establish
    break;
    
  case STATE_LOGIN:
    // Wait for user to log in
    break;
    
  case STATE_MEASURING:
    // Process measurements
    break;
    
  // Additional states...
}
```

## Best Practices for Contributors

When contributing to the HealthSense project, please follow these guidelines:

1. **Code Organization**: Keep related functionality together
2. **Error Handling**: Always check for error conditions
3. **Memory Management**: Free resources after use
4. **Documentation**: Document all public methods and complex logic
5. **Testing**: Test changes on actual hardware before submitting

## Troubleshooting for Developers

For common development issues, refer to [TROUBLESHOOTING.md](TROUBLESHOOTING.md).
