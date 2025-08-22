# HealthSense: ESP32 IoT Health Monitoring System

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

## Overview

HealthSense is an ESP32-based IoT health monitoring device that measures heart rate and blood oxygen saturation (SpO2) levels using a MAX30105 sensor. The system features a web interface for configuration, user authentication, and real-time measurement display, making health monitoring accessible and user-friendly.


## Features

- **WiFi Configuration**: Easy-to-use web interface for connecting to WiFi networks
- **User Authentication**: Secure login system with server-side authentication
- **Dual Operation Modes**:
  - **User Mode**: For registered users with data saving to the cloud
  - **Guest Mode**: For quick measurements without registration
- **Health Measurements**:
  - Heart rate monitoring (40-240 BPM)
  - Blood oxygen saturation (SpO2) measurement (0-100%)
- **Multi-platform Interface**:
  - Onboard ST7735 TFT display showing real-time measurements
  - Web interface for remote viewing and control via any browser
- **Reliable Measurement Process**:
  - Automated sensor calibration
  - Multi-reading averaging for accuracy
  - Advanced finger detection validation
  - Immediate browser redirection to results
- **Cloud Integration**:
  - Comprehensive REST API
  - Secure data storage for registered users
  - AI-based health analysis (User Mode only)

## System Architecture

HealthSense is built with a modular architecture consisting of three main components:

1. **WiFiManager**: Handles network connectivity, web server, and user interface
2. **SensorManager**: Controls the MAX30105 sensor, processes readings, and validates data
3. **DisplayManager**: Manages the TFT display and user interface elements

These components communicate through callbacks and a state machine in the main application, creating a flexible and maintainable system architecture.

## System Flow

### Initial Setup
1. When powered on, the ESP32 creates a WiFi access point named "HealthSense" (password: 123123123)
2. Connect to this access point and navigate to http://192.168.4.1
3. Enter your WiFi credentials to connect the device to your network

### User Authentication Flow
```
┌─────────┐     ┌──────────────┐     ┌──────────────┐     ┌────────────────┐
│ WiFi    │ ──► │ Mode         │ ──► │ User Login   │ ──► │ Measurement    │
│ Setup   │     │ Selection    │     │ Authentication│     │ Interface     │
└─────────┘     └──────────────┘     └──────────────┘     └────────────────┘
```

### Guest Mode Flow
```
┌─────────┐     ┌──────────────┐     ┌────────────────┐
│ WiFi    │ ──► │ Mode         │ ──► │ Measurement    │
│ Setup   │     │ Selection    │     │ Interface     │
└─────────┘     └──────────────┘     └────────────────┘
```

### Measurement Process Flow
```
┌─────────────┐     ┌──────────────┐     ┌──────────────┐     ┌────────────────┐
│ Start       │ ──► │ Reading      │ ──► │ Data         │ ──► │ Results        │
│ Measurement │     │ Collection   │     │ Processing   │     │ Display        │
└─────────────┘     └──────────────┘     └──────────────┘     └────────────────┘
```

## Comprehensive Documentation

HealthSense comes with detailed documentation to help users and developers:

- [Installation Guide](INSTALLATION.md): Setting up your development environment and building the firmware
- [Hardware Setup](HARDWARE_SETUP.md): Connecting the ESP32, MAX30105 sensor, and TFT display
- [Developer Guide](DEVELOPER_GUIDE.md): Code structure and implementation details
- [User Guide](USER_GUIDE.md): Instructions for end users on operating the device
- [Troubleshooting](TROUBLESHOOTING.md): Solutions to common issues
- [API Reference](API_REFERENCE.md): Comprehensive documentation of the device's REST API
- [Changelog](CHANGELOG.md): History of changes and version information
- [Contributing](CONTRIBUTING.md): Guidelines for contributing to the project
- [Code of Conduct](CODE_OF_CONDUCT.md): Community standards and expectations

## Hardware Requirements

- **Microcontroller**: ESP32 development board
- **Sensor**: MAX30105 pulse oximeter and heart rate sensor
- **Display**: ST7735 1.8" TFT LCD display (160x128 pixels)
- **Accessories**:
  - Breadboard for prototyping
  - Jumper wires
  - Micro USB cable for programming and power
  - Optional: 3.3V power supply
  - Optional: Case or enclosure

## Libraries Used

- **Adafruit ST7735 and ST7789 Library**: For TFT display control
- **SparkFun MAX3010x Pulse and Proximity Sensor Library**: For MAX30105 sensor interface
- **ArduinoJson**: For handling JSON data in API communication
- **ESP32 Arduino Core**: For core ESP32 functionality
- **WiFi and WebServer Libraries**: For network and web interface support

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Thanks to the ESP32 and Arduino communities
- SparkFun for the MAX3010x library
- Adafruit for the ST7735 display library
- All contributors to this project
