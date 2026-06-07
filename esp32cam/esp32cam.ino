/*
 * esp32cam.ino — LockEn face-capture node
 *
 * Board  : AI-Thinker ESP32-CAM
 * Library: ESP32 Arduino core (Board Manager: "esp32 by Espressif")
 *         Board setting: "AI Thinker ESP32-CAM"
 *
 * Captures JPEG frames and POSTs them to the laptop server.
 * The server keeps the latest frame for preview; face processing runs
 * only during enrollment or scan windows.
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
#define FLASH_LED_PIN    4

const char* ssid      = "LockEn_Safe";
const char* password  = "locken123";
const char* serverUrl = "http://192.168.4.2:8080/api/face/frame";

IPAddress localIP (192, 168, 4, 3);
IPAddress gateway (192, 168, 4, 1);
IPAddress subnet  (255, 255, 255, 0);
IPAddress dns     (192, 168, 4, 1);

static uint32_t frameCount = 0;
static uint32_t postOk     = 0;
static uint32_t postFail   = 0;

bool initCamera() {
    camera_config_t cfg = {};
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
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    cfg.pin_sccb_sda  = SIOD_GPIO_NUM;
    cfg.pin_sccb_scl  = SIOC_GPIO_NUM;
#else
    cfg.pin_sscb_sda  = SIOD_GPIO_NUM;
    cfg.pin_sscb_scl  = SIOC_GPIO_NUM;
#endif
    cfg.pin_pwdn      = PWDN_GPIO_NUM;
    cfg.pin_reset     = RESET_GPIO_NUM;
    cfg.xclk_freq_hz  = 20000000;
    cfg.pixel_format  = PIXFORMAT_JPEG;
    cfg.grab_mode     = CAMERA_GRAB_LATEST;

    if (psramFound()) {
        cfg.frame_size   = FRAMESIZE_VGA;   /* 640x480 — better for face detection */
        cfg.jpeg_quality = 10;
        cfg.fb_count     = 2;
        cfg.fb_location  = CAMERA_FB_IN_PSRAM;
        Serial.println("PSRAM found — using VGA frames in PSRAM");
    } else {
        cfg.frame_size   = FRAMESIZE_QVGA;  /* 320x240 fallback without PSRAM */
        cfg.jpeg_quality = 12;
        cfg.fb_count     = 1;
        cfg.fb_location  = CAMERA_FB_IN_DRAM;
        Serial.println("No PSRAM — using QVGA frames in DRAM");
    }

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("Camera init FAILED (0x%x)\n", err);
        return false;
    }

    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_brightness(sensor, 0);
        sensor->set_contrast(sensor, 0);
        sensor->set_saturation(sensor, 0);
        sensor->set_whitebal(sensor, 1);
        sensor->set_awb_gain(sensor, 1);
        sensor->set_exposure_ctrl(sensor, 1);
        sensor->set_gain_ctrl(sensor, 1);
    }

    return true;
}

bool connectWiFi() {
    WiFi.disconnect(true);
    delay(100);

    if (!WiFi.config(localIP, gateway, subnet, dns)) {
        Serial.println("Static IP config failed");
        return false;
    }

    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);

    Serial.print("Connecting to ");
    Serial.print(ssid);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connect FAILED");
        return false;
    }

    Serial.print("Connected — IP: ");
    Serial.println(WiFi.localIP());
    return WiFi.localIP() == localIP;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("LockEn ESP32-CAM starting");

    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW);

    if (!initCamera()) {
        Serial.println("Halting — fix camera wiring/power and reflash");
        while (true) delay(1000);
    }
    Serial.println("Camera ready");

    if (!connectWiFi()) {
        Serial.println("Will retry WiFi in loop");
    }
}

bool postFrame(camera_fb_t* fb) {
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(5000);

    if (!http.begin(serverUrl)) {
        Serial.println("HTTP begin failed");
        return false;
    }

    http.addHeader("Content-Type", "image/jpeg");
    int code = http.POST(fb->buf, fb->len);
    http.end();

    if (code == HTTP_CODE_OK) {
        return true;
    }

    Serial.printf("POST failed: HTTP %d (%s)\n", code, http.errorToString(code).c_str());
    return false;
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost — reconnecting");
        connectWiFi();
        delay(2000);
        return;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Frame capture failed");
        delay(200);
        return;
    }

    frameCount++;
    if (postFrame(fb)) {
        postOk++;
    } else {
        postFail++;
    }

    esp_camera_fb_return(fb);

    if (frameCount % 10 == 0) {
        Serial.printf("Frames: %lu  ok: %lu  fail: %lu  heap: %lu\n",
                      frameCount, postOk, postFail, ESP.getFreeHeap());
    }

    delay(400);
}
