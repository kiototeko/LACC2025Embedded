// -----------------------------------------------------------------------------
// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Include the HTTP server, timer, camera, image conversion, and graphics utilities
#include "esp_http_server.h"     // Provides HTTP server functions
#include "esp_timer.h"           // Provides high-resolution timing
#include "esp_camera.h"          // Camera driver interface
#include "img_converters.h"      // Convert raw frames to JPEG
#include "fb_gfx.h"              // Framebuffer graphics support
#include "driver/ledc.h"         // LED control via PWM

#include "sdkconfig.h"           // Project configuration
#include "Arduino.h"             // Arduino core functions (digitalWrite, Serial, etc.)

// Logging setup: choose Arduino or ESP-IDF style
#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
  #include "esp32-hal-log.h"      // Arduino-friendly logging macros
  #define TAG ""                  // Empty tag when using esp32-hal-log
#else
  #include "esp_log.h"            // ESP-IDF logging macros
  static const char *TAG = "camera_httpd"; // Tag for logs in ESP-IDF
#endif

// Define the on-board LED pin (GPIO2)
#define LED_PIN 2

// Structure to hold HTTP chunking state (unused here but defined for expansion)
typedef struct {
    httpd_req_t *req;   // HTTP request pointer
    size_t len;         // Length of the current chunk
} jpg_chunking_t;

// Boundary strings for multipart MJPEG streaming
#define PART_BOUNDARY "123456789000000000000987654321"
// Content-Type header for MJPEG stream
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
// Boundary marker between frames
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
// Header template for each JPEG frame (includes Content-Length and timestamp)
static const char *_STREAM_PART =
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n"
    "X-Timestamp: %d.%06d\r\n\r\n";

// Handles for the two HTTP servers (main and streaming)
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;


// Motion detection thresholds:
// DIFF_THRESHOLD: how much a pixel must change to count as motion
// MOTION_PIXELS: number of changed pixels to trigger "motion detected"
#define DIFF_THRESHOLD 30    
#define MOTION_PIXELS  100   



// Handler for JPEG streaming endpoint (/stream)
static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;        // Pointer for current frame buffer
    camera_fb_t *fb_last = NULL;   // Pointer for previous frame (for motion detection)
    struct timeval _timestamp;     // To hold frame timestamp
    esp_err_t res = ESP_OK;        // Result code
    size_t _jpg_buf_len = 0;       // Length of JPEG buffer
    uint8_t *_jpg_buf = NULL;      // Pointer to JPEG buffer
    char part_buf[128];            // Buffer to build the part header

    static int64_t last_frame = 0; // Timestamp of last sent frame
    if (!last_frame) {
        // Initialize on first call
        last_frame = esp_timer_get_time();
    }

    // Tell client we will send multipart MJPEG
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res; // If setting content type failed, exit
    }

    // Allow web pages from any origin (CORS header)
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    // Include a custom header to hint at desired frame rate
    httpd_resp_set_hdr(req, "X-Framerate", "60");

    // Main streaming loop: capture and send frames until error or disconnect
    while (true) {
        // Capture a frame from the camera
        
        fb = esp_camera_fb_get();
        
        if (!fb) {
            // Capture failed
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
        } else {
            // If we have a previous frame, perform simple motion detection
            if (fb_last) {
                ;
            }

            // Release the last frame buffer to free memory
            if (fb_last) {
                esp_camera_fb_return(fb_last);
            }
            fb_last = fb; // Save current as last

            // Record the frame's timestamp
            _timestamp.tv_sec  = fb->timestamp.tv_sec;
            _timestamp.tv_usec = fb->timestamp.tv_usec;

            // Convert to JPEG if frame is not already in JPEG format
            if (fb->format != PIXFORMAT_JPEG) {
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb); // Release original frame
                fb = NULL;
                if (!jpeg_converted) {
                    ESP_LOGE(TAG, "JPEG compression failed");
                    res = ESP_FAIL;
                }
            } else {
                // Frame is JPEG: use its buffer directly
                _jpg_buf_len = fb->len;
                _jpg_buf     = fb->buf;
            }
        }

        
        // If everything OK, send multipart boundary
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        // Send frame header (Content-Length and timestamp)
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, sizeof(part_buf),
                                _STREAM_PART,
                                _jpg_buf_len,
                                _timestamp.tv_sec,
                                _timestamp.tv_usec);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        // Send JPEG data
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        
        // Free or return buffers
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if (_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        // If an error occurred, break the loop
        if (res != ESP_OK) {
            ESP_LOGI(TAG, "res != ESP_OK : %d , break!", res);
            break;
        }

    }

    // Stream end: release last frame buffer
    if (fb_last) {
        esp_camera_fb_return(fb_last);
        fb_last = NULL;
    }
    last_frame = 0;
    return res;
}

// Parse URL query parameters for GET requests and store them in a buffer
static esp_err_t parse_get(httpd_req_t *req, char **obuf)
{
    char *buf = NULL;
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            *obuf = buf; // Caller must free
            return ESP_OK;
        }
        free(buf);
    }
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

// The HTML page served at the root: displays stream and save button
const char index_web[]=R"rawliteral(
<html>
  <head>
    <title>Video Streaming Demonstration</title>
  </head>
  <body>
    <!-- Page header -->
    <p><h1>Video Streaming Demonstration</h1></p>
    <!-- Image tag where MJPEG stream will appear -->
    <p><img id="stream" src="" style="transform:rotate(180deg)"/></p>

  </body>
  <script>
    // After page load, set stream source URL to port 81
    document.addEventListener('DOMContentLoaded', function (event) {
      var baseHost = document.location.origin;
      var streamUrl = baseHost + ':81';
      document.getElementById('stream').src = `${streamUrl}/stream`;
    });
  </script>
</html>)rawliteral";

// Handler for the root URL: serves the HTML page
static esp_err_t index_handler(httpd_req_t *req)
{
    esp_err_t err;
    Serial.printf("Serving index page\n"); // Debug print
    err = httpd_resp_set_type(req, "text/html");
    sensor_t *s = esp_camera_sensor_get(); // Check if camera works
    if (s != NULL) {
        // Send HTML content
        err = httpd_resp_send(req, index_web, sizeof(index_web));
    } else {
        ESP_LOGE(TAG, "Camera sensor not found");
        err = httpd_resp_send_500(req);
    }
    return err;
}


// Function to start both HTTP and streaming server 
void startCameraServer(){
    // Main server config (port 80)
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16; // Allow up to 16 URIs

    // Define URIs for index, stream, and button
    httpd_uri_t index_uri = {.uri = "/",    .method = HTTP_GET,  .handler = index_handler,  .user_ctx = NULL};
    httpd_uri_t stream_uri = {.uri = "/stream",.method = HTTP_GET,  .handler = stream_handler, .user_ctx = NULL};


    // Start main web server
    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        // Register handlers
        httpd_register_uri_handler(camera_httpd, &index_uri);
    }

    // Streaming server config (port 81)
    httpd_config_t cfg2 = HTTPD_DEFAULT_CONFIG();
    cfg2.server_port = 81;         // Streaming on port 81
    cfg2.ctrl_port   = 32769;      // Control port must differ
    cfg2.max_uri_handlers = 16;

    // Start streaming server
    ESP_LOGI(TAG, "Starting stream server on port: '%d'", cfg2.server_port);
    if (httpd_start(&stream_httpd, &cfg2) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}
