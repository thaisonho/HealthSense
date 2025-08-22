# HealthSense API Reference

This document provides a comprehensive reference for the HealthSense device's API endpoints and their usage. The API allows for remote interaction with the device, retrieving measurement data, and configuring settings.

## Table of Contents

- [Base URL](#base-url)
- [Authentication](#authentication)
- [Endpoints](#endpoints)
  - [Device Information](#device-information)
  - [WiFi Management](#wifi-management)
  - [Measurement Control](#measurement-control)
  - [Data Retrieval](#data-retrieval)
  - [System Control](#system-control)
- [Data Models](#data-models)
- [Error Handling](#error-handling)
- [Example Usage](#example-usage)
- [Integration Guide](#integration-guide)

## Base URL

When connected to the HealthSense device's access point:
```
http://192.168.4.1
```

When the device is connected to your WiFi network, use the assigned IP address:
```
http://{device-ip-address}
```

## Authentication

The HealthSense web API does not currently implement authentication. It's designed for local network use only. Future versions may implement token-based authentication for remote API access.

## Endpoints

### Device Information

#### GET /api/info

Returns basic information about the device.

**Response:**
```json
{
  "deviceName": "HealthSense",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "mac": "AB:CD:EF:12:34:56",
  "uptime": 3600
}
```

#### GET /api/status

Returns the current status of the device.

**Response:**
```json
{
  "state": "idle",  // idle, measuring, error
  "batteryLevel": 85,
  "memoryFree": 123456,
  "wifiConnected": true,
  "wifiSSID": "MyNetwork",
  "wifiRSSI": -65
}
```

### WiFi Management

#### GET /api/wifi/scan

Scans for available WiFi networks.

**Response:**
```json
{
  "networks": [
    {
      "ssid": "MyNetwork",
      "rssi": -65,
      "encryption": "WPA2"
    },
    {
      "ssid": "AnotherNetwork",
      "rssi": -72,
      "encryption": "WPA2"
    }
  ]
}
```

#### POST /api/wifi/connect

Connects to a WiFi network.

**Request:**
```json
{
  "ssid": "MyNetwork",
  "password": "MyPassword"
}
```

**Response:**
```json
{
  "status": "connecting",
  "message": "Attempting to connect to MyNetwork"
}
```

#### GET /api/wifi/status

Checks the status of the WiFi connection.

**Response:**
```json
{
  "connected": true,
  "ssid": "MyNetwork",
  "ip": "192.168.1.100",
  "rssi": -65,
  "mac": "AB:CD:EF:12:34:56"
}
```

#### POST /api/wifi/reset

Resets WiFi settings and restarts in AP mode.

**Response:**
```json
{
  "status": "success",
  "message": "WiFi settings reset. Device restarting..."
}
```

### Measurement Control

#### POST /api/measurement/start

Starts a new measurement.

**Request:**
```json
{
  "duration": 30,  // measurement duration in seconds (optional)
  "mode": "normal" // normal, extended, quick (optional)
}
```

**Response:**
```json
{
  "status": "started",
  "measurementId": "1234567890",
  "estimatedDuration": 30
}
```

#### GET /api/measurement/status

Gets the current measurement status.

**Response:**
```json
{
  "status": "measuring", // idle, measuring, complete, error
  "progress": 75, // percentage complete
  "timeRemaining": 7.5, // seconds
  "currentHeartRate": 72,
  "currentSpO2": 98,
  "error": null // error message if status is "error"
}
```

#### POST /api/measurement/stop

Stops the current measurement.

**Response:**
```json
{
  "status": "stopped",
  "message": "Measurement stopped by user"
}
```

### Data Retrieval

#### GET /api/measurement/latest

Returns the latest completed measurement data.

**Response:**
```json
{
  "measurementId": "1234567890",
  "timestamp": "2023-11-15T14:30:00Z",
  "duration": 30,
  "averageHeartRate": 72,
  "minHeartRate": 68,
  "maxHeartRate": 76,
  "averageSpO2": 98,
  "minSpO2": 97,
  "maxSpO2": 99,
  "confidence": 92,
  "samples": [
    {
      "timestamp": "2023-11-15T14:30:01Z",
      "heartRate": 70,
      "spO2": 98,
      "irValue": 12500,
      "redValue": 10000
    },
    // Additional samples...
  ]
}
```

#### GET /api/measurement/{id}

Returns a specific measurement by ID.

**Parameters:**
- `id`: Measurement ID

**Response:**
Same format as `/api/measurement/latest`

#### GET /api/measurements

Returns a list of recent measurements.

**Query Parameters:**
- `limit`: Maximum number of measurements to return (default: 10)
- `offset`: Number of measurements to skip (default: 0)

**Response:**
```json
{
  "measurements": [
    {
      "measurementId": "1234567890",
      "timestamp": "2023-11-15T14:30:00Z",
      "averageHeartRate": 72,
      "averageSpO2": 98
    },
    // Additional measurements...
  ],
  "total": 25,
  "limit": 10,
  "offset": 0
}
```

### System Control

#### POST /api/system/restart

Restarts the device.

**Response:**
```json
{
  "status": "restarting",
  "message": "Device will restart in 3 seconds"
}
```

#### GET /api/system/logs

Returns recent system logs.

**Query Parameters:**
- `lines`: Number of log lines to return (default: 50)

**Response:**
```json
{
  "logs": [
    {
      "timestamp": "2023-11-15T14:30:00Z",
      "level": "INFO",
      "message": "Device started"
    },
    // Additional log entries...
  ]
}
```

## Data Models

### MeasurementResult

| Field | Type | Description |
|-------|------|-------------|
| measurementId | string | Unique identifier for the measurement |
| timestamp | string (ISO 8601) | When the measurement was taken |
| duration | number | Measurement duration in seconds |
| averageHeartRate | number | Average heart rate in BPM |
| minHeartRate | number | Minimum heart rate in BPM |
| maxHeartRate | number | Maximum heart rate in BPM |
| averageSpO2 | number | Average SpO2 percentage |
| minSpO2 | number | Minimum SpO2 percentage |
| maxSpO2 | number | Maximum SpO2 percentage |
| confidence | number | Confidence score (0-100) |
| samples | array | Array of detailed sample readings |

### Sample

| Field | Type | Description |
|-------|------|-------------|
| timestamp | string (ISO 8601) | When the sample was taken |
| heartRate | number | Heart rate in BPM |
| spO2 | number | SpO2 percentage |
| irValue | number | IR sensor raw value |
| redValue | number | Red sensor raw value |

## Error Handling

All API endpoints return standard HTTP status codes:

- 200: Success
- 400: Bad Request - Invalid parameters
- 404: Not Found - Resource doesn't exist
- 500: Internal Server Error

Error responses include a JSON object with error details:

```json
{
  "error": {
    "code": "MEASUREMENT_IN_PROGRESS",
    "message": "Cannot start a new measurement while one is in progress"
  }
}
```

## Example Usage

### Starting a Measurement and Retrieving Results

```javascript
// Start a measurement
fetch('http://192.168.4.1/api/measurement/start', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json'
  },
  body: JSON.stringify({
    duration: 30
  })
})
.then(response => response.json())
.then(data => {
  console.log('Measurement started:', data);
  const measurementId = data.measurementId;
  
  // Poll for status until complete
  const statusCheck = setInterval(() => {
    fetch('http://192.168.4.1/api/measurement/status')
      .then(response => response.json())
      .then(statusData => {
        console.log('Progress:', statusData.progress + '%');
        
        if (statusData.status === 'complete') {
          clearInterval(statusCheck);
          
          // Get full results
          fetch(`http://192.168.4.1/api/measurement/${measurementId}`)
            .then(response => response.json())
            .then(measurementData => {
              console.log('Measurement results:', measurementData);
              // Process and display results
            });
        }
      });
  }, 1000);
});
```

## Integration Guide

### Integrating with External Systems

The HealthSense API can be integrated with:

1. **Mobile Applications**: Use the API to create native mobile apps that connect to the device
2. **Health Tracking Systems**: Export measurement data to health platforms
3. **Home Automation**: Trigger actions based on health readings

### Implementation Notes

- All API endpoints are relative to the base URL
- All timestamps are in ISO 8601 format (UTC)
- Measurements are stored in device memory and persist across reboots
- The device stores up to 10 measurements; older ones are deleted

### Development Environment

For development and testing, you can:

1. Connect to the device's access point
2. Use tools like Postman or curl to make API requests
3. Test integration with your application

### Future API Enhancements

Planned for future releases:

1. WebSocket support for real-time data streaming
2. Authentication for secure remote access
3. Extended historical data storage
4. Cloud synchronization capabilities
