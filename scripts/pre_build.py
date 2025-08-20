Import("env")
import os
import shutil
import re

# Function to modify MAX30105.h to avoid redefining I2C_BUFFER_LENGTH on ESP32
def modify_max30105_h(target, source, env):
    # Get the path to the MAX30105.h file
    max30105_h = os.path.join(env.get("PROJECT_LIBDEPS_DIR"), "esp32dev", 
                              "SparkFun MAX3010x Pulse and Proximity Sensor Library", 
                              "src", "MAX30105.h")
    
    if not os.path.exists(max30105_h):
        print(f"Could not find {max30105_h}")
        return
    
    # Create a backup
    backup_file = max30105_h + ".bak"
    if not os.path.exists(backup_file):
        shutil.copy2(max30105_h, backup_file)
    
    # Read the file content
    with open(max30105_h, "r") as f:
        content = f.read()
    
    # Check if we need to modify the file
    if "defined(ARDUINO_ARCH_ESP32)" not in content:
        # Add ESP32 check to avoid redefining I2C_BUFFER_LENGTH
        modified = re.sub(
            r'#elif defined\(__SAMD21G18A__\)(.*?)#else',
            '#elif defined(__SAMD21G18A__)\g<1>' +
            '#elif defined(ARDUINO_ARCH_ESP32) || defined(ESP32)\n' +
            '  // ESP32 already defines I2C_BUFFER_LENGTH in Wire.h, so we will use that value\n' +
            '  // No need to redefine\n' +
            '#else',
            content,
            flags=re.DOTALL
        )
        
        # Write the modified content back
        with open(max30105_h, "w") as f:
            f.write(modified)
        
        print(f"Modified {max30105_h} to avoid I2C_BUFFER_LENGTH redefinition on ESP32")

# Register the function to run before building
env.AddPreAction("$BUILD_DIR/src/main.cpp.o", modify_max30105_h)
