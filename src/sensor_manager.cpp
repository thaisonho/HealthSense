#include "sensor_manager.h"

SensorManager::SensorManager(int bufferSize) : 
    bufferLength(bufferSize),
    spo2(0),
    validSPO2(0),
    heartRate(0),
    validHeartRate(0),
    sensorReady(false),
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
    Wire.begin(sda_pin, scl_pin);
    sensorReady = false;
}

void SensorManager::initializeSensor() {
    // Initialize sensor 
    if (!particleSensor->begin(Wire, I2C_SPEED_FAST)) // Use default I2C port, 400kHz speed
    {
        Serial.println(F("MAX30105 was not found. Please check wiring/power."));
        sensorReady = false;
        return;
    }

    // First, turn on LEDs at maximum brightness for testing
    Serial.println(F("Setting up LEDs for always-on operation..."));
    
    // Configure in a simple way that keeps LEDs on
    byte ledBrightness = 0xFF; // Maximum brightness (0=Off to 255=50mA)
    byte sampleAverage = 1;    // Minimal averaging to ensure LEDs stay on
    byte ledMode = 3;          // All LEDs on (RED + IR + GREEN if available)
    byte sampleRate = 50;      // Lowest sample rate to keep LEDs on longer
    int pulseWidth = 411;      // Maximum pulse width
    int adcRange = 4096;       // Default ADC range
    
    // Basic setup with maximum brightness
    particleSensor->setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    
    // Force LEDs to full brightness
    particleSensor->setPulseAmplitudeRed(0xFF);   // Maximum RED LED brightness
    particleSensor->setPulseAmplitudeIR(0xFF);    // Maximum IR LED brightness
    particleSensor->setPulseAmplitudeGreen(0xFF); // Maximum GREEN LED brightness (not available on MAX30102)
    
    Serial.println(F("LEDs should now be on at maximum brightness."));
    Serial.println(F("Attach sensor to finger with rubber band. Press any key to start measurements"));
    
    // Wait for user input - but make it optional with a timeout
    unsigned long startTime = millis();
    while (Serial.available() == 0 && (millis() - startTime < 5000)) { 
        delay(100);
    }
    if (Serial.available() > 0) {
        Serial.read();
    }
    
    // Now adjust for better readings while keeping LEDs on
    ledBrightness = 60; // Reduced for better readings but still visible
    sampleAverage = 4;  // Default for readings
    ledMode = 2;        // RED + IR for SpO2
    sampleRate = 100;   // Standard sample rate for measurements
    
    particleSensor->setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); // Configure sensor with these settings
    
    // Keep LEDs bright enough to see but not too bright for good readings
    particleSensor->setPulseAmplitudeRed(0x30);   // Visible brightness for RED
    particleSensor->setPulseAmplitudeIR(0x30);    // Visible brightness for IR
    
    sensorReady = true;
}

void SensorManager::readSensor() {
    if (!sensorReady) return;
    
    // Read the first 100 samples, and determine the signal range
    for (byte i = 0; i < bufferLength; i++) {
        while (particleSensor->available() == false) // do we have new data?
            particleSensor->check(); // Check the sensor for new data

        redBuffer[i] = particleSensor->getRed();
        irBuffer[i] = particleSensor->getIR();
        particleSensor->nextSample(); // We're finished with this sample so move to next sample

        Serial.print(F("red="));
        Serial.print(redBuffer[i], DEC);
        Serial.print(F(", ir="));
        Serial.println(irBuffer[i], DEC);
    }

    // Calculate heart rate and SpO2 after first set of samples
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

void SensorManager::processReadings() {
    if (!sensorReady) return;
    
    // Dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = 25; i < 100; i++) {
        redBuffer[i - 25] = redBuffer[i];
        irBuffer[i - 25] = irBuffer[i];
    }

    // Take 25 sets of samples before calculating the heart rate.
    for (byte i = 75; i < 100; i++) {
        while (particleSensor->available() == false) // do we have new data?
            particleSensor->check(); // Check the sensor for new data

        redBuffer[i] = particleSensor->getRed();
        irBuffer[i] = particleSensor->getIR();
        particleSensor->nextSample(); // We're finished with this sample so move to next sample

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

    // After gathering 25 new samples recalculate HR and SP02
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

bool SensorManager::isFingerDetected() const {
    return (irBuffer[bufferLength - 1] > 50000) && (redBuffer[bufferLength - 1] > 50000);
}

void SensorManager::setUpdateReadingsCallback(void (*callback)(int32_t hr, bool validHR, int32_t spo2, bool validSPO2)) {
    updateReadingsCallback = callback;
}

void SensorManager::setUpdateFingerStatusCallback(void (*callback)(bool fingerDetected)) {
    updateFingerStatusCallback = callback;
}
