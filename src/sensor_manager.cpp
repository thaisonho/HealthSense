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
    validReadingCount(0),
    isMeasuring(false),
    averagedHR(0),
    averagedSpO2(0),
    measurementComplete(false),
    measurementStartTime(0),
    updateReadingsCallback(nullptr),
    updateFingerStatusCallback(nullptr),
    measurementCompleteCallback(nullptr) {
    
    particleSensor = new MAX30105();
    irBuffer = new uint32_t[bufferSize];
    redBuffer = new uint32_t[bufferSize];
    
    // Initialize valid readings array
    for (int i = 0; i < REQUIRED_VALID_READINGS; i++) {
        validReadings[i][0] = 0; // HR
        validReadings[i][1] = 0; // SpO2
    }
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

    Serial.println(F("Configuring sensor for optimal readings..."));
    
    // Use SparkFun example settings exactly
    byte ledBrightness = 60;    // SparkFun example value
    byte sampleAverage = 4;     // SparkFun example value  
    byte ledMode = 2;           // SparkFun example: RED + IR
    byte sampleRate = 100;      // SparkFun example value
    int pulseWidth = 411;       // SparkFun example value
    int adcRange = 4096;        // SparkFun example value
    
    // Configure sensor with SparkFun example settings
    particleSensor->setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    
    Serial.println(F("Sensor configured for optimal readings."));
    Serial.println(F("Place finger on sensor. Initializing in 3 seconds..."));
    
    // Allow time for setup
    delay(3000);
    
    Serial.println(F("Sensor initialized."));
    
    // Clear buffers before starting
    for (int i = 0; i < bufferLength; i++) {
        redBuffer[i] = 0;
        irBuffer[i] = 0;
    }
    
    sensorReady = true;
}

void SensorManager::readSensor() {
    if (!sensorReady) {
        resetSensor();
        return;
    }
    
    Serial.println(F("Starting initial sensor reading..."));
    
    // Follow SparkFun example exactly: read the first 100 samples to determine signal range
    for (byte i = 0; i < bufferLength; i++) {
        while (!particleSensor->available()) { // do we have new data?
            particleSensor->check(); // Check the sensor for new data
        }

        redBuffer[i] = particleSensor->getRed();
        irBuffer[i] = particleSensor->getIR();
        particleSensor->nextSample(); // We're finished with this sample so move to next sample

        Serial.print(F("red="));
        Serial.print(redBuffer[i], DEC);
        Serial.print(F(", ir="));
        Serial.println(irBuffer[i], DEC);
    }

    // Calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    
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
    
    // Reconfigure the sensor with the same settings as SparkFun example
    byte ledBrightness = 60;    // SparkFun example value
    byte sampleAverage = 4;     // SparkFun example value  
    byte ledMode = 2;           // SparkFun example: RED + IR
    byte sampleRate = 100;      // SparkFun example value
    int pulseWidth = 411;       // SparkFun example value
    int adcRange = 4096;        // SparkFun example value
    
    particleSensor->setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    
    // Clear the buffers
    for (int i = 0; i < bufferLength; i++) {
        redBuffer[i] = 0;
        irBuffer[i] = 0;
    }
    
    Serial.println(F("Sensor reset complete. Ready for measurements."));
    sensorReady = true;
}

void SensorManager::processReadings() {
    // Skip processing if measurement is already complete to prevent continued measurements
    if (measurementComplete) {
        Serial.println(F("🛑 Skipping processReadings() - measurement already complete"));
        return;
    }
    
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
    for (byte i = 75; i < 100; i++) {
        while (!particleSensor->available()) { // do we have new data?
            particleSensor->check(); // Check the sensor for new data
        }

        redBuffer[i] = particleSensor->getRed();
        irBuffer[i] = particleSensor->getIR();
        particleSensor->nextSample(); // We're finished with this sample so move to next sample

        // Send samples and calculation result to terminal program through UART
        Serial.print(F("red="));
        Serial.print(redBuffer[i], DEC);
        Serial.print(F(", ir="));
        Serial.print(irBuffer[i], DEC);

        // Only print HR and SpO2 values if they're valid
        if (isFingerDetected()) {
            Serial.print(F(", HR="));
            Serial.print(heartRate, DEC);
            
            Serial.print(F(", HRvalid="));
            Serial.print(validHeartRate, DEC);
            
            Serial.print(F(", SPO2="));
            Serial.print(spo2, DEC);
            
            Serial.print(F(", SPO2Valid="));
            Serial.println(validSPO2, DEC);
        } else {
            Serial.println(F(" - No finger detected"));
        }
    }
    
    // After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    
    // Check if a finger is actually detected BEFORE we validate any readings
    bool fingerPresent = isFingerDetected();
    if (!fingerPresent) {
        // If no finger detected, immediately mark readings as invalid
        validHeartRate = 0;
        validSPO2 = 0;
        Serial.println(F("No finger detected, marking readings as invalid"));
        return; // Skip further validation since there's no finger
    }
    
    // Additional validation for extreme HR values
    if (heartRate == -999) {
        validHeartRate = 0;
        Serial.print(F("Heart rate algorithm invalid ("));
        Serial.print(heartRate);
        Serial.println(F("), marked as invalid"));
    } else if (heartRate > MAX_VALID_HR || heartRate < MIN_VALID_HR) {
        validHeartRate = 0;
        Serial.print(F("Heart rate outside range ("));
        Serial.print(heartRate);
        Serial.println(F("), marked as invalid"));
    }
    
    // Additional validation for SpO2 values
    if (spo2 == -999) {
        validSPO2 = 0;
        Serial.print(F("SpO2 algorithm invalid ("));
        Serial.print(spo2);
        Serial.println(F("), marked as invalid"));
    } else if (spo2 > MAX_VALID_SPO2 || spo2 < MIN_VALID_SPO2) {
        validSPO2 = 0;
        Serial.print(F("SpO2 outside range ("));
        Serial.print(spo2);
        Serial.println(F("), marked as invalid"));
    }
    
    Serial.print(F("Calculated - HR="));
    Serial.print(heartRate);
    Serial.print(F(", HRvalid="));
    Serial.print(validHeartRate);
    Serial.print(F(", SPO2="));
    Serial.print(spo2);
    Serial.print(F(", SPO2Valid="));
    Serial.println(validSPO2);
    
    // Store current valid reading for display only if finger is present
    if (validHeartRate && validSPO2 && isFingerDetected()) {
        Serial.print(F("Current valid reading: HR="));
        Serial.print(heartRate);
        Serial.print(F(", SpO2="));
        Serial.println(spo2);
    }
    
    // Handle measurement averaging logic
    if (isMeasuring && !measurementComplete) {
        // Check for timeout
        if (millis() - measurementStartTime > MEASUREMENT_TIMEOUT_MS) {
            Serial.println(F("⏰ Measurement timeout! Could not get 5 valid readings in time."));
            Serial.print(F("Got "));
            Serial.print(validReadingCount);
            Serial.print(F("/"));
            Serial.print(REQUIRED_VALID_READINGS);
            Serial.println(F(" valid readings"));
            
            isMeasuring = false;
            measurementComplete = false;
            
            Serial.println(F("Please ensure finger is properly placed and try again."));
            return;
        }
        
        // Only add reading if both HR and SpO2 are valid AND finger is detected
        if (validHeartRate && validSPO2 && isFingerDetected()) {
            validReadings[validReadingCount][0] = heartRate;
            validReadings[validReadingCount][1] = spo2;
            validReadingCount++;
            
            Serial.print(F("✓ Valid reading "));
            Serial.print(validReadingCount);
            Serial.print(F("/"));
            Serial.print(REQUIRED_VALID_READINGS);
            Serial.print(F(": HR="));
            Serial.print(heartRate);
            Serial.print(F(", SpO2="));
            Serial.print(spo2);
            Serial.print(F(" (elapsed: "));
            Serial.print((millis() - measurementStartTime) / 1000);
            Serial.println(F("s)"));
            
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
                
                Serial.println(F("🎉 MEASUREMENT COMPLETE 🎉"));
                Serial.print(F("✅ Averaged HR: "));
                Serial.println(averagedHR);
                Serial.print(F("✅ Averaged SpO2: "));
                Serial.println(averagedSpO2);
                Serial.print(F("⏱️ Total time: "));
                Serial.print((millis() - measurementStartTime) / 1000);
                Serial.println(F(" seconds"));
                Serial.println(F("🎯 Calling measurement complete callback..."));
                
                // Call measurement complete callback
                if (measurementCompleteCallback) {
                    Serial.println(F("📞 Executing measurementCompleteCallback"));
                    measurementCompleteCallback(averagedHR, averagedSpO2);
                    Serial.println(F("✅ Callback execution complete"));
                } else {
                    Serial.println(F("❌ No measurementCompleteCallback registered!"));
                }
            }
        } else {
            // Continue measuring despite invalid reading
            Serial.print(F("✗ Invalid reading (HR="));
            Serial.print(heartRate);
            Serial.print(F(", valid="));
            Serial.print(validHeartRate);
            Serial.print(F(", SpO2="));
            Serial.print(spo2);
            Serial.print(F(", valid="));
            Serial.print(validSPO2);
            Serial.print(F(") - Progress: "));
            Serial.print(validReadingCount);
            Serial.print(F("/"));
            Serial.print(REQUIRED_VALID_READINGS);
            Serial.print(F(" (elapsed: "));
            Serial.print((millis() - measurementStartTime) / 1000);
            Serial.println(F("s)"));
            
            // Keep measuring! The measurement continues until we get 5 valid readings or timeout
        }
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
    
    // Set appropriate thresholds based on the typical signal levels observed in log
    uint32_t threshold_ir = IR_SIGNAL_THRESHOLD;
    uint32_t threshold_red = RED_SIGNAL_THRESHOLD;
    
    // We'll use more samples to make a more stable decision
    const int sampleCount = 25; // Use more samples for better detection
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
        Serial.print(F("🔍 Finger detection: Not enough valid samples ("));
        Serial.print(validSamples);
        Serial.println(F(")"));
        return false;
    }
    
    // Calculate the averages
    avgIR /= validSamples;
    avgRed /= validSamples;
    
    // Check if IR signal is in the expected range for a finger
    // and IR is significantly larger than red (typical for a finger on sensor)
    bool signalPresent = (avgIR > threshold_ir) && (avgRed > threshold_red);
    // For MAX30105, IR should be greater than RED but not by too much - adjust ratio check
    bool properRatio = (avgIR > avgRed * 0.9) && (avgIR < avgRed * 1.5);
    
    if (!signalPresent || !properRatio) {
        Serial.print(F("🔍 Finger detection failed - avgIR: "));
        Serial.print(avgIR);
        Serial.print(F(" (need >"));
        Serial.print(threshold_ir);
        Serial.print(F("), avgRed: "));
        Serial.print(avgRed);
        Serial.print(F(" (need >"));
        Serial.print(threshold_red);
        Serial.print(F("), IR/Red ratio: "));
        Serial.print(avgIR / (float)avgRed, 2);
        Serial.print(F(" (need 0.9-1.5), detection: "));
        Serial.println(properRatio ? "YES" : "NO");
    }
    
    return signalPresent && properRatio;
}

void SensorManager::setUpdateReadingsCallback(void (*callback)(int32_t hr, bool validHR, int32_t spo2, bool validSPO2)) {
    updateReadingsCallback = callback;
}

void SensorManager::setUpdateFingerStatusCallback(void (*callback)(bool fingerDetected)) {
    updateFingerStatusCallback = callback;
}

void SensorManager::setMeasurementCompleteCallback(void (*callback)(int32_t avgHR, int32_t avgSpO2)) {
    measurementCompleteCallback = callback;
}

void SensorManager::startMeasurement() {
    Serial.println(F("🔄 startMeasurement() called"));
    Serial.print(F("Current state - isMeasuring: "));
    Serial.print(isMeasuring);
    Serial.print(F(", validReadingCount: "));
    Serial.println(validReadingCount);
    
    Serial.println(F("Starting new measurement session..."));
    isMeasuring = true;
    measurementComplete = false;
    validReadingCount = 0;
    averagedHR = 0;
    averagedSpO2 = 0;
    measurementStartTime = millis();
    
    // Clear previous readings
    for (int i = 0; i < REQUIRED_VALID_READINGS; i++) {
        validReadings[i][0] = 0;
        validReadings[i][1] = 0;
    }
    
    Serial.print(F("Need "));
    Serial.print(REQUIRED_VALID_READINGS);
    Serial.print(F(" valid readings for averaging (timeout: "));
    Serial.print(MEASUREMENT_TIMEOUT_MS / 1000);
    Serial.println(F(" seconds)..."));
    
    // Make sure sensor is ready
    if (!sensorReady) {
        Serial.println(F("⚠️ Sensor not ready! Initializing..."));
        initializeSensor();
    }
    
    Serial.println(F("✅ Measurement started!"));
}

void SensorManager::stopMeasurement() {
    Serial.println(F("🔄 stopMeasurement() called"));
    isMeasuring = false;
    measurementComplete = false;
    validReadingCount = 0;
}
