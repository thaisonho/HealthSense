#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

// Device-specific identifiers
#define DEVICE_ID "esp"  
#define DEVICE_SECRET "ngotantai"
#define USER_ID "default_user_id"

// App state enum shared across files
enum AppState {
  STATE_SETUP,
  STATE_CONNECTING,
  STATE_LOGIN,
  STATE_MEASURING,
  STATE_AI_ANALYSIS
};

#endif // COMMON_TYPES_H