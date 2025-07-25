# Video Web Server for ESP32-S3 Camera Module

Welcome! This project turns an ESP32-S3 camera into a simple webpage that shows live video and lights an LED when it detects motion. It's designed for high school students to learn how cameras, WiFi, and basic web servers work on embedded devices.

## What’s Inside

- `VideoWebServer.ino`: The Arduino sketch that:

  - Configures and initializes the camera hardware.
  - Connects to a WiFi network.
  - Starts two HTTP servers for serving a webpage and streaming video.

- `app_httpd.cpp`: The implementation file that:

  - Defines HTTP request handlers for a webpage and MJPEG stream.
  - Contains the motion detection logic by comparing video frames.

---

## Hardware Requirements

1. **ESP32-S3 board** with a camera.
2. USB cable to power and program the board.
3. Arduino IDE with ESP32 board support installed.
4. `camera_pins.h` header file that maps camera pins on your board.

---

## Overview of Operation

### 1. Arduino Sketch (`.ino` file)

0. **LED Output Setup**: The sketch uses `#define LED_PIN 2` at the top and configures it as an output inside `setup()`. This LED lights up when motion is detected.

   ```cpp
   pinMode(LED_PIN, OUTPUT);
   ```
1. **Camera Setup**: The sketch creates a `camera_config_t` structure with settings such as pin assignments (from `camera_pins.h`), frame size, pixel format, and frame buffer count. It then calls `esp_camera_init(&config)` to start the camera driver.

   ```cpp
   camera_config_t config;
   config.ledc_channel = LEDC_CHANNEL_0;
   config.ledc_timer   = LEDC_TIMER_0;
   config.pin_d0       = Y2_GPIO_NUM;
   // ... (other pin assignments from camera_pins.h) ...
   config.frame_size   = FRAMESIZE_QQVGA;
   config.pixel_format = PIXFORMAT_GRAYSCALE;
   config.fb_count     = 2;

   esp_err_t err = esp_camera_init(&config);
   if (err != ESP_OK) {
     Serial.println("Camera init failed!");
     return;
   }
   ```

2. **WiFi Connection**: It uses `WiFi.begin(ssid, password)` to join your network. A small loop checks `WiFi.status()` and waits until the board is connected.

   ```cpp
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print(".");
   }
   Serial.println("WiFi connected!");
   ```




3. **Server Startup**: After WiFi is ready, the sketch calls `startCameraServer()`, which is defined in `app_httpd.cpp`.

```cpp
startCameraServer();
```

4. **Main Loop**: The `loop()` function is empty because the servers run independently.
   ```cpp
   void loop() {
     // Servers run in background — no code needed here.
   }
   ```

### 2. HTTP Server Implementation (app_httpd.cpp)

#### 1. These definitions appear near the top of `app_httpd.cpp`:

```cpp
// Define the on-board LED pin (GPIO2)
#define LED_PIN 2

// Motion detection thresholds:
#define DIFF_THRESHOLD 30    // Pixel difference threshold
#define MOTION_PIXELS  100   // Number of pixels that must change

// MJPEG streaming boundaries:
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY =
    "\r\n--" PART_BOUNDARY "\r\n";
```

- `LED_PIN`: Controls the on-board LED.
- `DIFF_THRESHOLD`: Minimum change in pixel intensity to count.
- `MOTION_PIXELS`: How many changed pixels trigger motion.
- **Boundary constants**: Used to separate JPEG frames in the HTTP stream.

---

#### 2. Webpage Handler: `index_handler`

```cpp
static esp_err_t index_handler(httpd_req_t *req)
{
    esp_err_t err;
    Serial.printf("Serving index page\n");
    err = httpd_resp_set_type(req, "text/html");
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        err = httpd_resp_send(req, index_web, sizeof(index_web));
    } else {
        err = httpd_resp_send_500(req);
    }
    return err;
}
```

- **Purpose**: Serves the HTML page (`index_web`) containing the video `<img>` element and a button.
- **Camera check**: Ensures `esp_camera_sensor_get()` returns valid sensor before sending content.

---

#### 3. Stream Handler with Motion Detection: `stream_handler`

The `stream_handler` function runs continuously to capture camera frames, detect motion by comparing current and previous frames, and send JPEG images over HTTP as an MJPEG stream. Below is the core code snippet, followed by a detailed breakdown.

```cpp
static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    camera_fb_t *fb_last = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[128];

    // 1️⃣ Set response headers for MJPEG streaming
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");

    // 2️⃣ Main loop: keep sending frames until error or stop
    while (true) {
        fb = esp_camera_fb_get();  // Capture a new frame
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
        } else {
            // --- Motion Detection ---
            if (fb_last) {
                int changed = 0;
                uint8_t *data  = fb->buf;
                uint8_t *data0 = fb_last->buf;
                size_t len     = fb->len;
                // 3️⃣ Compare pixel values every 4th byte for speed
                for (size_t i = 0; i < len; i += 4) {
                    if (abs(data[i] - data0[i]) > DIFF_THRESHOLD) {
                        if (++changed > MOTION_PIXELS) {
                            break;  // Motion detected, no need to count further
                        }
                    }
                }
                // 4️⃣ Update LED based on motion count
                if (changed > MOTION_PIXELS) {
                    digitalWrite(LED_PIN, HIGH);
                } else {
                    digitalWrite(LED_PIN, LOW);
                }
                // 5️⃣ Release previous frame to free memory
                esp_camera_fb_return(fb_last);
            }
            fb_last = fb;

            // --- JPEG Conversion & Streaming ---
            // 6️⃣ Ensure frame is JPEG format for streaming
            if (fb->format != PIXFORMAT_JPEG) {
                frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf     = fb->buf;
            }

            // 7️⃣ Send multipart boundary to separate frames
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY,
                                        strlen(_STREAM_BOUNDARY));
            // 8️⃣ Send frame headers (content type and length)
            size_t hlen = snprintf(part_buf, sizeof(part_buf),
                                   "Content-Type: image/jpeg
Content-Length: %u

",
                                   (unsigned)_jpg_buf_len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
            // 9️⃣ Send the JPEG buffer
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf,
                                        _jpg_buf_len);
        }
        // 10️⃣ Exit loop on error
        if (res != ESP_OK) {
            break;
        }
    }

    // 11️⃣ Cleanup: return last frame buffer and reset timestamp
    if (fb_last) {
        esp_camera_fb_return(fb_last);
    }
    last_frame = 0;
    return res;
}
```

**Step-by-Step Explanation:**

1. **Set up HTTP response**: Configure the response as an MJPEG stream with appropriate headers.
2. **Enter main loop**: Continuously capture and send frames.
3. **Frame capture**: Use `esp_camera_fb_get()` to grab the latest image buffer.
4. **Motion detection logic**:
   - If a **previous frame** exists, compare pixel values at intervals (every 4th byte) to speed up processing.
   - Count how many pixels differ by more than `DIFF_THRESHOLD`.
   - If the count exceeds `MOTION_PIXELS`, motion is detected.
5. **LED indicator**: Turn the LED **ON** when motion exceeds threshold; otherwise, **OFF**.
6. **Memory management**: Return the previous frame buffer to free resources.
7. **JPEG preparation**: Convert non-JPEG frames to JPEG with `frame2jpg`, or use the buffer directly if already JPEG.
8. **Multipart streaming**:
   - Send a boundary marker (`_STREAM_BOUNDARY`).
   - Send HTTP headers for the image chunk (content type and length).
   - Send the JPEG image data.
9. **Loop control**: Continue until an error occurs, then exit the loop.
10. **Cleanup**: Release the last frame buffer and reset any timestamps before returning.


---

#### 4. Server Startup: `startCameraServer`

```cpp
void startCameraServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;

    httpd_uri_t index_uri = {
        .uri = "/", .method = HTTP_GET,
        .handler = index_handler, .user_ctx = NULL
    };
    httpd_uri_t stream_uri = {
        .uri = "/stream", .method = HTTP_GET,
        .handler = stream_handler, .user_ctx = NULL
    };

    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
    }

    httpd_config_t cfg2 = HTTPD_DEFAULT_CONFIG();
    cfg2.server_port = 81;
    cfg2.ctrl_port   = 32769;
    ESP_LOGI(TAG, "Starting stream server on port: '%d'", cfg2.server_port);
    if (httpd_start(&stream_httpd, &cfg2) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}
```

- **Main Server**: Port **80** serves the HTML page.
- **Stream Server**: Port **81** handles the MJPEG video stream.

---

## How to Run

1. **Edit** the `ssid` and `password` variables in the Arduino sketch to match your WiFi network.
2. **Upload** both files (`.ino` and `app_httpd.cpp`) to your ESP32-S3 board using the Arduino IDE.
3. **Open** the Serial Monitor at 115200 baud to view the board’s IP address.
4. **Navigate** to `http://<board_ip>` in a web browser on the same network to see the live video feed.
5. **Test Motion**: Move an object or person in front of the camera; you should see the on-board LED light up when motion is detected.

---

## Further Exploration

- **Adjust Sensitivity**: Try changing the motion threshold constant in the original `app_httpd.cpp` file to tune detection.
- **Higher Resolution**: Experiment with different `frame_size` values for better image clarity.
- **Color Output**: Switch from grayscale to color by changing `pixel_format` to `PIXFORMAT_RGB565` or similar.
- **Web Controls**: Expand the HTML page with buttons or sliders to control parameters like JPEG quality or LED behavior.

Enjoy experimenting with embedded web servers and simple motion detection!
