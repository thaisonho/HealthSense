# HealthSense Troubleshooting Guide

This guide provides solutions for common issues encountered when setting up, programming, or using the HealthSense device.

## Development and Upload Issues

### Compilation Errors

#### I2C_BUFFER_LENGTH Redefinition Error

**Error Message:**
```
Multiple definition of 'I2C_BUFFER_LENGTH'
```

**Solution:**
1. Locate MAX30105.h in the SparkFun MAX3010x library
2. Edit the file to add ESP32 exception as described in [MAX30105_MANUAL_FIX.md](MAX30105_MANUAL_FIX.md)
3. Alternatively, add the following to your platformio.ini:
   ```ini
   build_flags = -DI2C_BUFFER_LENGTH_DEFINED
   ```

#### Library Not Found Errors

**Error Message:**
```
Fatal Error: No such file or directory: Adafruit_ST7735.h
```

**Solution:**
1. Check that all required libraries are listed in platformio.ini
2. Run `pio lib install` in the terminal
3. Restart PlatformIO IDE

### Upload Problems

#### Cannot Connect to ESP32

**Error Message:**
```
A fatal error occurred: Failed to connect to ESP32: Timed out waiting for packet header
```

**Solution:**
1. Try the manual upload method:
   - Hold the BOOT button
   - Press and release RESET
   - Release BOOT after 1-2 seconds
   - Run upload again
2. Check your USB cable (some are charge-only)
3. Try a different USB port
4. Ensure you have the correct board selected in platformio.ini

#### Wrong Upload Port

**Error Message:**
```
Serial port COM4 not found
```

**Solution:**
1. Check which COM port your ESP32 is connected to:
   - Windows: Device Manager > Ports (COM & LPT)
   - Mac/Linux: Run `ls /dev/tty*` in terminal
2. Update your platformio.ini file:
   ```ini
   upload_port = COM4  # Replace with your port
   ```

## Hardware Issues

### MAX30105 Sensor Not Detected

**Error Message:**
```
MAX30105 was not found. Please check wiring/power.
```

**Solution:**
1. Check all connections between ESP32 and MAX30105:
   - SDA to GPIO21
   - SCL to GPIO22
   - VIN to 3.3V
   - GND to GND
2. Verify that you're using 3.3V (not 5V) to power the sensor
3. Try running an I2C scanner sketch to verify the sensor is detected
4. Ensure the sensor is not damaged (no bent pins)

### Display Not Working

**Symptoms:**
- Blank screen
- Garbled display
- Wrong colors

**Solution:**
1. Verify all connections to the ST7735 display:
   - CS to GPIO5
   - DC to GPIO2
   - RESET to GPIO4
   - SDA(MOSI) to GPIO23
   - SCK to GPIO18
2. Check that the display is powered (3.3V and GND connected)
3. Try initializing the display with different settings in display_manager.cpp
4. Check for proper initialization sequence

## WiFi Connection Issues

### Cannot Create Access Point

**Symptoms:**
- No "HealthSense" WiFi network appears
- Serial monitor shows AP creation failure

**Solution:**
1. Check power supply - AP mode requires sufficient power
2. Ensure ESP32 is properly flashed with the firmware
3. Try resetting the device
4. Check serial output for specific error messages

### Cannot Connect to User's WiFi Network

**Symptoms:**
- Device shows "Connection Failed" message
- Returns to AP mode after connection attempt

**Solution:**
1. Verify credentials are correct (case-sensitive)
2. Ensure the network is 2.4GHz (ESP32 doesn't support 5GHz networks)
3. Check if your network has special characters in SSID or password
4. Move closer to your router
5. Check router settings for MAC filtering or client limits

### Web Interface Not Accessible

**Symptoms:**
- Cannot access http://192.168.4.1 when connected to HealthSense network
- Or cannot access the device when connected to home network

**Solution:**
1. Verify you're connected to the correct network
2. Try accessing the device by IP address
3. Clear browser cache or try incognito/private mode
4. Try a different browser
5. Check if your computer's firewall is blocking the connection

## Measurement Issues

### No Valid Readings

**Symptoms:**
- "No finger detected" message persists
- Measurement times out without collecting readings

**Solution:**
1. Ensure finger is correctly placed on the sensor:
   - Cover the entire sensor surface
   - Apply gentle pressure
   - Keep finger still
2. Avoid bright ambient light on the sensor
3. Check if sensor LEDs are illuminating when finger is placed
4. Try with different finger or different person
5. Ensure sensor is clean and undamaged

### Inaccurate Readings

**Symptoms:**
- Readings fluctuate significantly
- Values seem unrealistic (HR < 40 or > 200, SpO2 < 80%)

**Solution:**
1. Improve finger placement and reduce movement
2. Warm your hands if they are cold
3. Try calibrating the sensor by adjusting LED brightness in sensor_manager.cpp
4. Check for interference from bright lights
5. Ensure proper signal levels by monitoring the raw IR and RED values

### Browser Not Redirecting to Results

**Symptoms:**
- Measurement completes but browser stays on measuring page
- Have to manually navigate to results page

**Solution:**
1. Check JavaScript console for errors
2. Ensure your browser allows redirects
3. Try a different browser
4. Verify that the ESP32's WiFi connection remains stable
5. Look for error messages in the serial monitor

## System Operation Issues

### Device Crashes or Resets

**Symptoms:**
- ESP32 restarts unexpectedly
- Web interface becomes unresponsive
- Display freezes

**Solution:**
1. Check power supply stability
2. Monitor memory usage - add Serial.printf("Free heap: %d\n", ESP.getFreeHeap()) to identify memory leaks
3. Add delay(10) in the main loop to prevent watchdog timer issues
4. Reduce buffer sizes if memory is an issue
5. Check for hardware issues like loose connections

### Cannot Switch Between Modes

**Symptoms:**
- Device gets stuck in one mode
- Cannot access mode selection page

**Solution:**
1. Try the "Mode Select" button on various pages
2. Restart the device
3. Clear browser cache and cookies
4. Reset WiFi settings by choosing "Reconfigure WiFi"
5. Check the serial monitor for error messages

## API Integration Issues

### Authentication Fails

**Symptoms:**
- Cannot log in with valid credentials
- "Login failed" message appears

**Solution:**
1. Verify server URL in the code
2. Check if the API server is online
3. Verify your account credentials
4. Look for error responses in Serial monitor
5. Test API endpoint separately using tools like Postman

### Measurement Data Not Saved

**Symptoms:**
- Measurements complete but don't appear in user account
- API errors in serial monitor

**Solution:**
1. Ensure you're using User Mode (not Guest Mode)
2. Check WiFi connectivity during measurement
3. Verify API endpoint URLs
4. Check authentication token validity
5. Ensure proper JSON format for API requests

## Advanced Debugging

### Serial Monitoring

For advanced troubleshooting, connect to the serial monitor at 115200 baud rate:

1. In PlatformIO IDE, click the "Serial Monitor" button
2. Set baud rate to 115200
3. Monitor for error messages and debug output

### Debug Build

To enable additional debugging output:

1. Add to platformio.ini:
   ```ini
   build_flags = -DDEBUG
   ```
2. Use the debug outputs to identify issues

### Reset EEPROM

If the device gets stuck in an unrecoverable state:

1. Add this code to a temporary sketch:
   ```cpp
   #include <EEPROM.h>
   
   void setup() {
     EEPROM.begin(1024);
     for (int i = 0; i < 1024; i++) {
       EEPROM.write(i, 0);
     }
     EEPROM.commit();
     EEPROM.end();
     Serial.begin(115200);
     Serial.println("EEPROM cleared!");
   }
   
   void loop() {}
   ```
2. Upload and run to reset all saved settings

## Reporting Issues

If you encounter issues not covered in this guide:

1. Document the specific problem
2. Note any error messages from the serial monitor
3. Describe the steps to reproduce the issue
4. Include hardware configuration details
5. Submit an issue on the GitHub repository or contact support

## Common Error Codes

| Code | Description | Solution |
|------|-------------|----------|
| WL_NO_SHIELD | WiFi hardware not found | Check ESP32 hardware |
| WL_NO_SSID_AVAIL | SSID not available | Verify network name |
| WL_CONNECT_FAILED | Connection failed | Check password or router settings |
| HTTP_CODE_UNAUTHORIZED | API authentication failed | Check credentials |
| HTTP_CODE_NOT_FOUND | API endpoint not found | Verify server URL |

For additional help, refer to the [ESP32 Technical Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/) or contact project support.
