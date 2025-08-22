# HealthSense Installation Guide

This guide covers the process of setting up your development environment and installing the HealthSense firmware on an ESP32.

## Development Environment Setup

### Prerequisites
- Computer with Windows, macOS, or Linux
- Internet connection for downloading libraries and tools
- Git installed (for cloning the repository)
- VSCode editor recommended

### Step 1: Install PlatformIO

PlatformIO is the recommended IDE for working with the HealthSense project.

1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Open VSCode and go to the Extensions view (click the square icon on the left sidebar or press Ctrl+Shift+X)
3. Search for "PlatformIO IDE" and click Install
4. Wait for the installation to complete and restart VSCode when prompted

### Step 2: Clone the Repository

1. Open a terminal or command prompt
2. Navigate to your preferred directory for storing projects
3. Clone the repository:
   ```
   git clone https://github.com/pnthai285/HealthSense.git
   ```
4. Change to the project directory:
   ```
   cd HealthSense
   ```

### Step 3: Open the Project in PlatformIO

1. Open VSCode
2. Click the PlatformIO icon in the left sidebar
3. Select "Open Project" and navigate to the cloned HealthSense directory
4. Click "Open" to load the project

### Step 4: Install Dependencies

PlatformIO will automatically install most dependencies defined in the `platformio.ini` file. However, you may need to fix an issue with the MAX30105 library:

#### Fix MAX30105 Library for ESP32

The MAX30105 library has a known issue with ESP32 boards related to the I2C_BUFFER_LENGTH definition. Follow these steps to fix it:

1. Locate the MAX30105.h file in your project's library dependencies:
   - Path: `.pio/libdeps/esp32dev/SparkFun MAX3010x Pulse and Proximity Sensor Library/src/MAX30105.h`

2. Open this file in an editor

3. Find the section around line 42 that defines I2C_BUFFER_LENGTH. It should look like this:
   ```cpp
   //Define the size of the I2C buffer based on the platform the user has
   #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)

     //I2C_BUFFER_LENGTH is defined in Wire.H
     #define I2C_BUFFER_LENGTH BUFFER_LENGTH

   #elif defined(__SAMD21G18A__)

     //SAMD21 uses RingBuffer.h
     #define I2C_BUFFER_LENGTH SERIAL_BUFFER_SIZE

   #else

     //The catch-all default is 32
     #define I2C_BUFFER_LENGTH 32

   #endif
   ```

4. Modify this section to add a check for ESP32:
   ```cpp
   //Define the size of the I2C buffer based on the platform the user has
   #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)

     //I2C_BUFFER_LENGTH is defined in Wire.H
     #define I2C_BUFFER_LENGTH BUFFER_LENGTH

   #elif defined(__SAMD21G18A__)

     //SAMD21 uses RingBuffer.h
     #define I2C_BUFFER_LENGTH SERIAL_BUFFER_SIZE

   #elif defined(ARDUINO_ARCH_ESP32) || defined(ESP32) || defined(I2C_BUFFER_LENGTH_DEFINED)
     
     // ESP32 already defines I2C_BUFFER_LENGTH in Wire.h, so we'll use that value
     // No need to redefine

   #else

     //The catch-all default is 32
     #define I2C_BUFFER_LENGTH 32

   #endif
   ```

5. Save the file

### Step 5: Customize Configuration (Optional)

If needed, you can modify the project configuration:

1. Open the `platformio.ini` file in the root of the project
2. Adjust settings like upload port, monitor speed, etc.
3. Example configuration:
   ```ini
   [env:esp32dev]
   platform = espressif32
   board = esp32dev
   framework = arduino
   lib_deps = 
       adafruit/Adafruit ST7735 and ST7789 Library@^1.11.0
       sparkfun/SparkFun MAX3010x Pulse and Proximity Sensor Library@^1.1.2
       bblanchon/ArduinoJson@^6.21.3
   upload_port = COM4  # Change to your ESP32's COM port
   upload_speed = 921600
   monitor_speed = 115200
   ```

## Building and Uploading

### Step 1: Build the Project

1. In PlatformIO, click on the checkmark icon in the bottom toolbar (or press Ctrl+Alt+B)
2. Wait for the build process to complete
3. Check for any compilation errors

### Step 2: Upload to ESP32

#### Normal Upload Method

1. Connect your ESP32 to your computer via USB
2. In PlatformIO, click on the right arrow icon in the bottom toolbar (or press Ctrl+Alt+U)
3. Wait for the upload process to complete

#### Manual Upload Method (If Normal Method Fails)

If you have trouble uploading, try the manual upload method:

1. Connect your ESP32 to your computer via USB
2. Put your ESP32 into download mode:
   - Press and hold the BOOT button (labeled "BOOT" or "IO0")
   - While holding BOOT, press and release the RESET button (labeled "EN" or "RST")
   - Release the BOOT button after 1-2 seconds
3. In PlatformIO, click on the right arrow icon to start the upload

### Step 3: Verify Installation

1. After successful upload, open the Serial Monitor in PlatformIO (press Ctrl+Alt+S)
2. Reset your ESP32 by pressing the RESET button
3. You should see output indicating that the device is starting and creating a WiFi access point

## Next Steps

After successful installation:

1. Connect to the "HealthSense" WiFi network using password "123123123"
2. Navigate to http://192.168.4.1 in your web browser
3. Follow the on-screen instructions to configure your device

## Troubleshooting

### Common Issues

#### Upload Fails with Connection Error
- Make sure you've selected the correct COM port
- Try the manual upload method described above
- Check your USB cable (some cables are charge-only)
- Try a different USB port on your computer

#### Compilation Errors Related to I2C_BUFFER_LENGTH
- Follow the MAX30105 library fix described above
- Or add this line to platformio.ini under build_flags:
  ```
  build_flags = -DI2C_BUFFER_LENGTH_DEFINED
  ```

#### Device Connects but Web Interface Doesn't Load
- Make sure your computer has fully connected to the HealthSense WiFi network
- Try accessing http://192.168.4.1 directly
- Some mobile devices might require you to disable mobile data to properly use the captive portal

For more detailed troubleshooting, refer to [Troubleshooting](TROUBLESHOOTING.md).
