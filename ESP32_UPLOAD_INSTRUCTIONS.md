# ESP32 Upload Instructions

## Manual Upload Mode

If you're having trouble uploading to your ESP32, try these steps:

1. **Connect your ESP32 to your computer via USB**

2. **Enter Download Mode**:
   - Press and hold the BOOT button (labeled "BOOT" or "IO0")
   - While holding BOOT, press and release the RESET button (labeled "EN" or "RST")
   - Release the BOOT button after 1-2 seconds

3. **Upload your code** while the ESP32 is in download mode

## Hardware Troubleshooting

If you're still having issues:

1. **Try a different USB cable** - Some cables are charge-only
2. **Use a different USB port** - Some ports may not provide enough power
3. **Check your drivers** - Make sure you have the CP210x or CH340 drivers installed
4. **Look at the onboard LEDs** - Some boards have LEDs that indicate power and connection status

## Platformio.ini Configuration

Update your platformio.ini file with these settings for better upload reliability:

```ini
upload_port = COM4  # Change this to your actual COM port
upload_speed = 921600
monitor_speed = 115200

upload_flags = 
    --before=default_reset
    --after=hard_reset
```
