# Manual Fix for MAX30105.h I2C_BUFFER_LENGTH Redefinition

To fix the I2C_BUFFER_LENGTH redefinition warning without using Python scripts, follow these steps:

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

4. Modify this section to add a check for ESP32 and our I2C_BUFFER_LENGTH_DEFINED flag:
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

This change ensures that when using ESP32, the code won't try to redefine I2C_BUFFER_LENGTH.

Note: You'll need to make this change each time you update the SparkFun MAX3010x Pulse and Proximity Sensor Library.
