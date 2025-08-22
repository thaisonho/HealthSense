# MQTT Integration for HealthSense Project

This document explains how the MQTT functionality was integrated into the HealthSense project.

## Integration Overview

The HealthSense project now includes MQTT connectivity to a HiveMQ Cloud MQTT broker. This allows the ESP32 to:

1. Connect to a secure MQTT broker using TLS/SSL
2. Subscribe to a topic that matches its device ID with QoS level 1
3. Play a notification sound on the buzzer when a message is received
4. Skip notification if a measurement is in progress

## MQTT Settings

- **MQTT Broker**: 70030b8b8dc741c79d6ab7ffa586f461.s1.eu.hivemq.cloud
- **Port**: 8883 (TLS/SSL)
- **Username**: phamngocthai
- **Password**: Thai2005
- **QoS Level**: 1

## Implementation Details

1. Added two libraries to the project:
   - PubSubClient: For MQTT communication
   - ArduinoMqttClient: As an alternative if needed

2. Created a new `MQTTManager` class to handle:
   - Connection to the MQTT broker
   - Subscription to topics
   - Message processing
   - Playing notifications

3. Connected the MQTT functionality to the existing measurement system:
   - The device checks if a measurement is in progress before playing notifications
   - The buzzer plays a notification melody for approximately 8 seconds

## Testing the MQTT Integration

To test the MQTT functionality, you can use the HiveMQ Console or MQTT.fx client to publish messages to the topic that matches the device ID.

1. Connect to broker: 70030b8b8dc741c79d6ab7ffa586f461.s1.eu.hivemq.cloud:8883
2. Use the same credentials: Username: phamngocthai, Password: Thai2005
3. Publish a message to topic "esp32_health_sense" (the device ID)
4. The payload should be a timestamp value
5. Set QoS level to 1

The ESP32 should receive the message and play a notification sound if it is not currently performing a measurement.

## Code Structure Changes

- Added MQTT broker connection details in `mqtt_manager.h`
- Created notification melody in `mqtt_manager.cpp`
- Integrated with main application flow in `main.cpp`
- Defined a buzzer pin (pin 25) for the notification system
- Set up a callback to check if measurement is in progress

## Hardware Setup

The buzzer should be connected to pin 25 on the ESP32.

## Debug Output

The implementation provides detailed debug output over Serial to track:
- MQTT connection status
- Topic subscription status
- Received messages
- Measurement status checks
- Notification triggers

## Future Enhancements

Future versions could expand the MQTT functionality to:
- Send measurement results via MQTT
- Add more sophisticated commands via MQTT
- Implement bidirectional communication with the cloud service
