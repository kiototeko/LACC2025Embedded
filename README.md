# LACC2025Embedded

## ESP32-S3 Arduino IDE Setup Guide

Follow these steps to install the Arduino IDE, add ESP32 board support, and select the ESP32-S3 board.

---

### 1. Install (or update) the Arduino IDE

1. Go to the [Arduino Software page](https://arduino.cc/en/software).  
2. Download the latest Arduino IDE for your operating system (Windows/macOS/Linux).  
3. Install and launch the Arduino IDE.

---

### 2. Add ESP32 board support

1. **Open Preferences**  
   - In the Arduino IDE, navigate to **File → Preferences**.  
2. **Add the ESP32 URL**  
   - Find the field labeled **Additional Boards Manager URLs**.  
   - Add the following URL:  
     ```text
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Click **OK** to save.
3. **Install ESP32 cores**  
   - Go to **Tools → Board → Boards Manager…**  
   - In the search box, type **esp32**.  
   - Locate **esp32 by Espressif Systems** and click **Install version 3.2.1**
   - Wait for the installation to finish.

---

### 3. Select the ESP32-S3 Board

1. **Choose your board**  
   - Go to **Tools → Board → ESP32 Arduino → ESP32S3 Dev Module**  
   - This option covers the ESP32-S3 WROOM and most S3 variants.
2. **Set Upload Speed**  
   - Under **Tools → Upload Speed**, select **115200** (or higher, e.g., 230400).
3. **Select the correct Port**  
   - Under **Tools → Port**, choose the COM (or `/dev/ttyUSB*`) port associated with your ESP32-S3 module.

---

### 4. Enable OPI PSRAM

1. **Tools** -> **PSRAM** -> **OPI PSRAM**

### 5. Open Code and Upload

1. **Open Arduino_Camera.ino file** 
2. **Upload file into ESP32 board**

---

## ESP32-S3 Tutorial

You can download all the information and example code regarding the Freenove ESP32-S3-WROOM Board in [here](https://freenove.com/fnk0085).

---

## Troubleshooting

`A fatal error occurred: Failed to write to target RAM (result was 01070000: Operation timed out)`

Install [https://github.com/WCHSoftGroup/ch34xser_macos](https://github.com/WCHSoftGroup/ch34xser_macos)
