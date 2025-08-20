/*
 * ESP32 compatibility fix for SparkFun MAX3010x Pulse and Proximity Sensor Library
 * 
 * This file ensures that the ESP32's I2C_BUFFER_LENGTH from Wire.h is used
 * rather than the default value in MAX30105.h to avoid redefinition warnings.
 */

#ifndef ESP32_MAX30105_FIX_H
#define ESP32_MAX30105_FIX_H

// Make sure Wire.h is included first so I2C_BUFFER_LENGTH is defined
#include <Wire.h>

// Define a flag to indicate that I2C_BUFFER_LENGTH is already defined
#define I2C_BUFFER_LENGTH_DEFINED

#endif // ESP32_MAX30105_FIX_H
