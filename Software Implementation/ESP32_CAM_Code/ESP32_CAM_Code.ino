/* Arduino Code For ESP32-CAM */

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_http_server.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "img_converters.h"

// WiFi credentials 
const char* ssid     = "Vinod";
const char* password = "Vinod@1234";

//  Python Flask server 
const char* LAPTOP_IP   = "10.253.170.18";  
const char* SERVER_PORT = "5000";

//  GPIO 
#define LED_GREEN   2
#define LED_RED     12
#define BUZZER_PIN  13
#define GAS_DOUT    14

//ESP32-CAM pin map (AI-Thinker) 
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

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

String gasResult    = "FRESH";
String aiResult     = "UNKNOWN";
String finalResult  = "FRESH";
float  aiConfidence = 0.0;
bool   serverOnline = false;

#define AI_INTERVAL 5000
unsigned long lastAITime = 0;

// data endpoint (JSON) 
esp_err_t data_handler(httpd_req_t *req) {
  String json = "{";
  json += "\"gas_result\":\""  + gasResult                     + "\",";
  json += "\"ai_result\":\""   + aiResult                      + "\",";
  json += "\"ai_confidence\":" + String(aiConfidence, 1)       + ",";
  json += "\"final_result\":\"" + finalResult                  + "\",";
  json += "\"server_online\":";
  json += (serverOnline ? "true" : "false");
  json += "}";

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_sendstr(req, json.c_str());
  return ESP_OK;
}

//  MJPEG stream endpoint 
#define PART_BOUNDARY "123456789000000000000987654321"

esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb  = NULL;
  char         part_buf[128];

  httpd_resp_set_type(req,
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY);
  // FIX: CORS so browser doesn't block the img src
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      break;
    }

    size_t hlen = snprintf(part_buf, sizeof(part_buf),
      "\r\n--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
      PART_BOUNDARY, (uint32_t)fb->len);

    esp_err_t res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res == ESP_OK)
      res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);

    esp_camera_fb_return(fb);

    if (res != ESP_OK) break;   // client disconnected
  }
  return ESP_OK;
}

// Start HTTP servers 
void startCameraServer() {
  // --- Port 80: data JSON ---
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port   = 32768;   // unique control port

  httpd_uri_t data_uri = { "/data", HTTP_GET, data_handler, NULL };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &data_uri);
    Serial.printf("Data server on port 80\n");
  }

  //  Port 81: MJPEG stream
  httpd_config_t stream_config = HTTPD_DEFAULT_CONFIG();
  stream_config.server_port = 81;
  stream_config.ctrl_port   = 32769;

  httpd_uri_t stream_uri = { "/stream", HTTP_GET, stream_handler, NULL };

  if (httpd_start(&stream_httpd, &stream_config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.printf("Stream server on port 81\n");
  }
}

// Capture frame → send to Flask → parse result 
void runAICheck() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  HTTPClient http;
  String url = String("http://") + LAPTOP_IP + ":" + SERVER_PORT + ":/analyse";
  http.begin(url);
  http.addHeader("Content-Type", "image/jpeg");
  http.setTimeout(8000);

  int code = http.POST(fb->buf, fb->len);

  if (code == 200) {
    String response = http.getString();
    Serial.println("AI response: " + response);

    if (response.indexOf("MOLDY") >= 0) {
    aiResult = "MOLDY";
} else if (response.indexOf("FRESH") >= 0) {
    aiResult = "FRESH";
}

if (gasResult == "SPOILED" || aiResult == "MOLDY") {
    finalResult = "SPOILED";
} else {
    finalResult = "FRESH";
}
    int ci = response.indexOf("\"confidence\":");
    if (ci >= 0) {
      aiConfidence = response.substring(ci + 13).toFloat();
    }

    serverOnline = true;
  } else {
    Serial.printf("AI server returned %d\n", code);
    serverOnline = false;
  }

  http.end();
  esp_camera_fb_return(fb);

  // FIX: sync finalResult (was never updated in original)
  if (gasResult == "SPOILED" || aiResult == "MOLDY") {
    finalResult = "SPOILED";
  } else {
    finalResult = "FRESH";
  }
}

//  SETUP 
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
 Serial.println("=== BOOT ===");
 esp_reset_reason_t reason = esp_reset_reason();
 Serial.printf("Reset reason: %d\n", reason);

  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GAS_DOUT,   INPUT);

  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  WiFi.begin(ssid, password);

int retry = 0;
while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
  retry++;
  if (retry > 20) {
    Serial.println("\nWiFi FAILED!");
    break;
  }
}

if (WiFi.status() == WL_CONNECTED) {
  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}
delay(2000);

  // Camera config
  camera_config_t cam;
  cam.ledc_channel = LEDC_CHANNEL_0;
  cam.ledc_timer   = LEDC_TIMER_0;
  cam.pin_d0       = Y2_GPIO_NUM;
  cam.pin_d1       = Y3_GPIO_NUM;
  cam.pin_d2       = Y4_GPIO_NUM;
  cam.pin_d3       = Y5_GPIO_NUM;
  cam.pin_d4       = Y6_GPIO_NUM;
  cam.pin_d5       = Y7_GPIO_NUM;
  cam.pin_d6       = Y8_GPIO_NUM;
  cam.pin_d7       = Y9_GPIO_NUM;
  cam.pin_xclk     = XCLK_GPIO_NUM;
  cam.pin_pclk     = PCLK_GPIO_NUM;
  cam.pin_vsync    = VSYNC_GPIO_NUM;
  cam.pin_href     = HREF_GPIO_NUM;
  cam.pin_sscb_sda = SIOD_GPIO_NUM;
  cam.pin_sscb_scl = SIOC_GPIO_NUM;
  cam.pin_pwdn     = PWDN_GPIO_NUM;
  cam.pin_reset    = RESET_GPIO_NUM;
  cam.xclk_freq_hz = 20000000;
  cam.pixel_format = PIXFORMAT_JPEG;
  cam.frame_size   = FRAMESIZE_QQVGA;
  cam.jpeg_quality = 18;
  cam.fb_count     = 2;

  esp_err_t err = esp_camera_init(&cam);
  if (err != ESP_OK) {
    Serial.printf("Camera init FAILED: 0x%x\n", err);
    return;
  }
  Serial.println("Camera Ready!");

  startCameraServer();
  Serial.println("HTTP servers started.");
  Serial.printf("  Data   → http://%s/data\n",   WiFi.localIP().toString().c_str());
  Serial.printf("  Stream → http://%s:81/stream\n", WiFi.localIP().toString().c_str());
}
//  LOOP 
void loop() {

  int dout = digitalRead(GAS_DOUT);

  if (dout == HIGH) {

    gasResult = "SPOILED";
    finalResult = "SPOILED";

    digitalWrite(LED_RED, HIGH);
    delay(2000);
    digitalWrite(LED_GREEN, LOW);
    delay(2000);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(2000);

  } else {

    gasResult = "FRESH";
    finalResult = "FRESH";

    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  if (millis() - lastAITime > AI_INTERVAL) {
    lastAITime = millis();
    runAICheck();
  }

  delay(100);
}