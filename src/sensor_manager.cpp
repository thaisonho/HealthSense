#include "sensor_manager.h"

SensorManager::SensorManager(int bufferSize) : 
    bufferLength(bufferSize),
    spo2(0),
    validSPO2(0),
    heartRate(0),
    validHeartRate(0),
    sensorReady(false),
    lastI2CErrorTime(0),
    i2cErrorCount(0),
    sda_pin(0),
    scl_pin(0),
    updateReadingsCallback(nullptr),
    updateFingerStatusCallback(nullptr) {
    
    particleSensor = new MAX30105();
    irBuffer = new uint32_t[bufferSize];
    redBuffer = new uint32_t[bufferSize];
}

SensorManager::~SensorManager() {
    delete particleSensor;
    delete[] irBuffer;
    delete[] redBuffer;
}

void SensorManager::begin(int sda_pin, int scl_pin) {
    this->sda_pin = sda_pin;
    this->scl_pin = scl_pin;
    Wire.begin(sda_pin, scl_pin);
    sensorReady = false;
    lastI2CErrorTime = millis();
    i2cErrorCount = 0;
}

void SensorManager::initializeSensor() {
    // Initialize sensor 
    if (!particleSensor->begin(Wire, I2C_SPEED_FAST)) // Use default I2C port, 400kHz speed
    {
        Serial.println(F("MAX30105 was not found. Please check wiring/power."));
        sensorReady = false;
        return;
    }

    // Configure sensor with optimal settings for SpO2 and HR measurements
    Serial.println(F("Configuring sensor for optimal readings..."));
    
    // Use consistent settings optimized for accurate measurements using predefined constants
    byte ledBrightness = LED_BRIGHTNESS_DEFAULT;  // Reduced brightness to avoid saturation
    byte sampleAverage = SAMPLE_AVERAGE;          // Average samples for better noise reduction
    byte ledMode = LED_MODE_SPO2;                 // Use RED + IR for SpO2 measurement
    byte sampleRate = SAMPLE_RATE;                // Sampling rate - good balance for HR detection
    int pulseWidth = PULSE_WIDTH;                 // Maximum pulse width for better sensitivity
    int adcRange = ADC_RANGE;                     // Default ADC range
    
    // Initialize with these consistent settings
    particleSensor->setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    
    // Critically reduce LED brightness to prevent sensor saturation
    particleSensor->setPulseAmplitudeRed(LED_BRIGHTNESS_LOW);   // Reduced RED LED brightness
    particleSensor->setPulseAmplitudeIR(LED_BRIGHTNESS_LOW);    // Reduced IR LED brightness
    particleSensor->setPulseAmplitudeGreen(0);                  // Turn off GREEN LED completely
    
    Serial.println(F("Sensor configured for optimal readings."));
    Serial.println(F("Place finger on sensor. Initializing in 3 seconds..."));
    
    // Allow time for setup without requiring user input
    delay(3000);
    
    Serial.println(F("Sensor initialized."));
    
    // Check initial readings and adjust if necessary
    if (!particleSensor->available()) {
        particleSensor->check();
    }
    
    uint32_t initialRed = particleSensor->getRed();
    uint32_t initialIR = particleSensor->getIR();
    
    Serial.print(F("Initial readings - RED: "));
    Serial.print(initialRed);
    Serial.print(F(", IR: "));
    Serial.println(initialIR);
    
    // If readings are still too high, adjust further
    if (initialRed > SIGNAL_SATURATION_LIMIT || initialIR > SIGNAL_SATURATION_LIMIT) {
        particleSensor->setPulseAmplitudeRed(LED_BRIGHTNESS_VERY_LOW);   // Further reduce if needed
        particleSensor->setPulseAmplitudeIR(LED_BRIGHTNESS_VERY_LOW);    // Further reduce if needed
        Serial.println(F("Brightness reduced further to prevent saturation"));
    }
    
    sensorReady = true;
}

void SensorManager::readSensor() {
    if (!sensorReady) {
        resetSensor();
        return;
    }
    
    Serial.println(F("Starting initial sensor reading..."));
    int errorCount = 0;
    
    // Read the first 100 samples, and determine the signal range
    for (byte i = 0; i < bufferLength; i++) {
        bool readSuccess = false;
        int retryCount = 0;
        
        // Try to read the sensor with retries
        while (!readSuccess && retryCount < 3) {
            try {
                if (!particleSensor->available()) {
                    particleSensor->check();
                    // Give it a little time
                    delay(10);
                }
                
                if (particleSensor->available()) {
                    redBuffer[i] = particleSensor->getRed();
                    irBuffer[i] = particleSensor->getIR();
                    particleSensor->nextSample();
                    readSuccess = true;
                }
            } catch (...) {
                errorCount++;
                delay(10);
            }
            
            retryCount++;
        }
        
        // If read failed after retries and we have previous data, use that
        if (!readSuccess && i > 0) {
            redBuffer[i] = redBuffer[i-1];
            irBuffer[i] = irBuffer[i-1];
        }

        // Only print every 10th reading to reduce serial output
        if (i % 10 == 0) {
            Serial.print(F("red="));
            Serial.print(redBuffer[i], DEC);
            Serial.print(F(", ir="));
            Serial.println(irBuffer[i], DEC);
        }
    }
    
    // Check if we had too many errors
    if (errorCount > 20) {
        Serial.println(F("Too many errors during initial reading. Resetting sensor..."));
        resetSensor();
        return;
    }

    // Calculate heart rate and SpO2 after first set of samples
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    
    // Additional validation for extreme values
    if (heartRate > MAX_VALID_HR || heartRate < MIN_VALID_HR) {
        validHeartRate = 0;
    }
    
    // Update the readings via callback
    if (updateReadingsCallback) {
        updateReadingsCallback(heartRate, validHeartRate, spo2, validSPO2);
    }
    
    // Update finger status via callback
    if (updateFingerStatusCallback) {
        updateFingerStatusCallback(isFingerDetected());
    }
}

bool SensorManager::checkI2CConnection() {
    // Record I2C error time for rate limiting resets
    unsigned long currentTime = millis();
    
    // Don't check too frequently - wait at least 5 seconds between resets
    if (currentTime - lastI2CErrorTime < 5000) {
        return sensorReady;
    }
    
    // Try to read from the sensor's ID register (0xFF)
    Wire.beginTransmission(0x57); // MAX30105 I2C address
    Wire.write(0xFF);             // ID register
    byte error = Wire.endTransmission();
    
    // If we get an error, increment the counter
    if (error != 0) {
        i2cErrorCount++;
        lastI2CErrorTime = currentTime;
        Serial.print(F("I2C Error: "));
        Serial.println(error);
        
        // If we've seen multiple errors, reset the connection
        if (i2cErrorCount >= 3) {
            resetSensor();
            i2cErrorCount = 0;
        }
        
        return false;
    }
    
    // No error, reset the counter
    i2cErrorCount = 0;
    return true;
}

void SensorManager::resetSensor() {
    Serial.println(F("Attempting to reset sensor connection..."));
    
    // Reset the I2C connection
    Wire.end();
    delay(100);
    Wire.begin(sda_pin, scl_pin);
    Wire.flush();
    delay(100);
    
    // Try to reinitialize the sensor
    if (!particleSensor->begin(Wire, I2C_SPEED_FAST)) {
        Serial.println(F("Failed to reinitialize sensor. Will retry later."));
        sensorReady = false;
        return;
    }
    
    // Reconfigure the sensor with the same settings
    byte ledBrightness = LED_BRIGHTNESS_DEFAULT;
    byte sampleAverage = SAMPLE_AVERAGE;
    byte ledMode = LED_MODE_SPO2;
    byte sampleRate = SAMPLE_RATE;
    int pulseWidth = PULSE_WIDTH;
    int adcRange = ADC_RANGE;
    
    particleSensor->setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    particleSensor->setPulseAmplitudeRed(LED_BRIGHTNESS_LOW);
    particleSensor->setPulseAmplitudeIR(LED_BRIGHTNESS_LOW);
    particleSensor->setPulseAmplitudeGreen(0);
    
    // Clear the buffers
    for (int i = 0; i < bufferLength; i++) {
        redBuffer[i] = 0;
        irBuffer[i] = 0;
    }
    
    Serial.println(F("Sensor reset complete. Ready for measurements."));
    sensorReady = true;
}

void SensorManager::processReadings() {
    if (!sensorReady) {
        // Try to reinitialize sensor if it's not ready
        resetSensor();
        return;
    }
    
    // Dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = 25; i < 100; i++) {
        redBuffer[i - 25] = redBuffer[i];
        irBuffer[i - 25] = irBuffer[i];
    }

    // Take 25 sets of samples before calculating the heart rate.
    int errorCount = 0;
    for (byte i = 75; i < 100; i++) {
        bool readSuccess = false;
        int retryCount = 0;
        
        // Try to read the sensor with retries
        while (!readSuccess && retryCount < 3) {
            try {
                // Check if sensor has data
                if (!particleSensor->available()) {
                    particleSensor->check();
                    // Wait a bit for data to become available
                    delay(10);
                }
                
                if (particleSensor->available()) {
                    redBuffer[i] = particleSensor->getRed();
                    irBuffer[i] = particleSensor->getIR();
                    particleSensor->nextSample();
                    readSuccess = true;
                }
            } catch (...) {
                // Catch any exceptions during sensor reading
                Serial.println(F("Error reading sensor data"));
                errorCount++;
                delay(10);
            }
            
            retryCount++;
        }
        
        // If we couldn't read after retries, use previous values
        if (!readSuccess) {
            if (i > 0) {
                redBuffer[i] = redBuffer[i-1];
                irBuffer[i] = irBuffer[i-1];
            }
        }
        
        // Send samples and calculation result to terminal program through UART
        Serial.print(F("red="));
        Serial.print(redBuffer[i], DEC);
        Serial.print(F(", ir="));
        Serial.print(irBuffer[i], DEC);

        Serial.print(F(", HR="));
        Serial.print(heartRate, DEC);

        Serial.print(F(", HRvalid="));
        Serial.print(validHeartRate, DEC);

        Serial.print(F(", SPO2="));
        Serial.print(spo2, DEC);

        Serial.print(F(", SPO2Valid="));
        Serial.println(validSPO2, DEC);
    }
    
    // Check if we encountered too many errors
    if (errorCount > 10) {
        Serial.println(F("Too many sensor read errors. Resetting sensor..."));
        resetSensor();
        return;
    }

    // After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    
    // Additional validation for extreme HR values
    if (heartRate > MAX_VALID_HR || heartRate < MIN_VALID_HR) {
        // If heart rate is outside physiological range, mark it as invalid
        validHeartRate = 0;
        Serial.println(F("Heart rate outside physiological range, marked as invalid"));
    }
    
    // Update the readings via callback
    if (updateReadingsCallback) {
        updateReadingsCallback(heartRate, validHeartRate, spo2, validSPO2);
    }
    
    // Update finger status via callback
    if (updateFingerStatusCallback) {
        updateFingerStatusCallback(isFingerDetected());
    }
}

bool SensorManager::isFingerDetected() const {
    // Check if sensor is ready
    if (!sensorReady) {
        return false;
    }
    
    // We'll use the last 10 samples to make a more stable decision
    const int sampleCount = 10;
    int startIdx = (bufferLength > sampleCount) ? bufferLength - sampleCount : 0;
    
    // Calculate average values
    uint32_t avgIR = 0;
    uint32_t avgRed = 0;
    int validSamples = 0;
    
    for (int i = startIdx; i < bufferLength; i++) {
        // Check if this is a valid sample (non-zero and not abnormally high)
        if (irBuffer[i] > 0 && redBuffer[i] > 0 && 
            irBuffer[i] < SIGNAL_SATURATION_LIMIT && redBuffer[i] < SIGNAL_SATURATION_LIMIT) {
            avgIR += irBuffer[i];
            avgRed += redBuffer[i];
            validSamples++;
        }
    }
    
    // If we don't have enough valid samples, finger is not detected
    if (validSamples < 3) {
        return false;
    }
    
    // Calculate the averages
    avgIR /= validSamples;
    avgRed /= validSamples;
    
    // Check if IR signal is in the expected range for a finger
    // and IR is significantly larger than red (typical for a finger on sensor)
    bool signalPresent = (avgIR > IR_SIGNAL_THRESHOLD) && (avgRed > RED_SIGNAL_THRESHOLD);
    bool properRatio = (avgIR > avgRed * 0.8); // IR should be larger than red for a finger
    
    return signalPresent && properRatio;
}

void SensorManager::setUpdateReadingsCallback(void (*callback)(int32_t hr, bool validHR, int32_t spo2, bool validSPO2)) {
    updateReadingsCallback = callback;
}

void SensorManager::setUpdateFingerStatusCallback(void (*callback)(bool fingerDetected)) {
    updateFingerStatusCallback = callback;
}
