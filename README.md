# HealthSense ESP32 Health Monitor

## Overview
HealthSense is an ESP32-based health monitoring device that measures heart rate and SpO2 levels using a MAX30105 sensor. The device features a web interface for configuration and two operation modes: User Mode and Guest Mode.

## Features
- WiFi configuration interface
- User authentication through web interface
- Guest mode for quick measurements without account
- Real-time heart rate and SpO2 measurement
- Data transmission to a server API (in User Mode)
- TFT display for measurement readings
- WiFi reconfiguration option on all pages

## Operation Modes

### WiFi Configuration
1. When powered on, the ESP32 creates a WiFi access point named "HealthSense"
2. Connect to this access point using the password "123123123"
3. Access the configuration page at http://192.168.4.1
4. Enter your WiFi credentials to connect the device to your network

### User Mode
1. After connecting to WiFi, select "User Mode" on the mode selection page
2. Enter your email and password
3. The device authenticates with the server API
4. If successful, it begins measuring heart rate and SpO2
5. Measurement data is sent to the server API and displayed on the device's screen

### Guest Mode
1. After connecting to WiFi, select "Guest Mode" on the mode selection page
2. The device immediately begins measuring without requiring authentication
3. Measurements are displayed on the device's screen but not saved to any server
4. A link to register for an account is provided

## WiFi Reconfiguration
- On any page, you can access the "Reconfigure WiFi" option
- This stops any ongoing measurements and returns to the WiFi configuration page

## Server API Integration
The device integrates with a server API for:
1. User authentication (POST /api/authenticate)
2. Sending measurement data (POST /api/measurements)

## Hardware
- ESP32 microcontroller
- MAX30105 pulse oximeter sensor
- ST7735 TFT display

## Libraries Used
- Adafruit ST7735 and ST7789 Library
- SparkFun MAX3010x Pulse and Proximity Sensor Library
- ArduinoJson
