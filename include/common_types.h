#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

// Measurement phases for result accuracy and reliability
enum MeasurementPhase {
    PHASE_INIT = 0,      // Phase 1: Initialization/Noise (first 1-3 seconds)
    PHASE_STABILIZE,     // Phase 2: Signal Stabilization (next 3-8 seconds)
    PHASE_RELIABLE       // Phase 3: Reliable Results (after 5-10 seconds)
};

// Constants for staged measurement logic
#define SETTLING_TIME_MS 3000
#define STABILIZE_TIME_MS 5000
#define REQUIRED_VALID_COUNT 4

#endif // COMMON_TYPES_H
