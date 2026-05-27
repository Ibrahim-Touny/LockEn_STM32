/*
 * esp32cam.ino — LockEn face-capture node
 *
 * Board  : AI-Thinker ESP32-CAM
 * Library: ESP32 Arduino core (install via Board Manager: "esp32 by Espressif")
 *
 * Continuously captures JPEG frames and POSTs them to the laptop server.
 * The server discards frames when not enrolling or scanning — no power waste.
 *
 * Static IP: 192.168.4.3  (gateway = ESP8266 AP at 192.168.4.1)
 * Server   : http://192.168.4.2:8080/api/face/frame
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

/* ── AI-Thinker ESP32-CAM pin map ── */
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

const char* ssid      = "LockEn_Safe";
const char* password  = "locken123";
const char* serverUrl = "http://192.168.4.2:8080/api/face/frame";

/* Static IP so the camera is always reachable at a known address */
IPAddress localIP (192, 168, 4, 3);
IPAddress gateway (192, 168, 4, 1);
IPAddress subnet  (255, 255, 255, 0);

void setup() {
    Serial.begin(115200);

    /* Camera init */
    camera_config_t cfg;
    cfg.ledc_channel  = LEDC_CHANNEL_0;
    cfg.ledc_timer    = LEDC_TIMER_0;
    cfg.pin_d0        = Y2_GPIO_NUM;
    cfg.pin_d1        = Y3_GPIO_NUM;
    cfg.pin_d2        = Y4_GPIO_NUM;
    cfg.pin_d3        = Y5_GPIO_NUM;
    cfg.pin_d4        = Y6_GPIO_NUM;
    cfg.pin_d5        = Y7_GPIO_NUM;
    cfg.pin_d6        = Y8_GPIO_NUM;
    cfg.pin_d7        = Y9_GPIO_NUM;
    cfg.pin_xclk      = XCLK_GPIO_NUM;
    cfg.pin_pclk      = PCLK_GPIO_NUM;
    cfg.pin_vsync     = VSYNC_GPIO_NUM;
    cfg.pin_href      = HREF_GPIO_NUM;
    cfg.pin_sscb_sda  = SIOD_GPIO_NUM;
    cfg.pin_sscb_scl  = SIOC_GPIO_NUM;
    cfg.pin_pwdn      = PWDN_GPIO_NUM;
    cfg.pin_reset     = RESET_GPIO_NUM;
    cfg.xclk_freq_hz  = 20000000;
    cfg.pixel_format  = PIXFORMAT_JPEG;
    cfg.frame_size    = FRAMESIZE_QVGA;  /* 320×240 — good balance for face_recognition */
    cfg.jpeg_quality  = 12;              /* 0=best, 63=worst */
    cfg.fb_count      = 1;

    if (esp_camera_init(&cfg) != ESP_OK) {
        Serial.println("Camera init FAILED");
        while (true) delay(1000);
    }
    Serial.println("Camera ready");

    /* WiFi connect with static IP */
    WiFi.config(localIP, gateway, subnet);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to " + String(ssid));
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
    }
    Serial.println("\nConnected — IP: " + WiFi.localIP().toString());
}

void loop() {
    /* Reconnect if dropped */
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost — reconnecting");
        WiFi.reconnect();
        delay(3000);
        return;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        delay(100);
        return;
    }

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "image/jpeg");
    http.setTimeout(3000);

    int code = http.POST(fb->buf, fb->len);
    if (code < 0) {
        Serial.println("POST failed: " + http.errorToString(code));
    }
    http.end();

    esp_camera_fb_return(fb);

    delay(500);   /* ~2 fps — enough for face recognition, low enough to not flood the server */
}
