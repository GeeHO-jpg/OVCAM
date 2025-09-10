#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "esp_camera.h"
#include "esp32-hal-psram.h"
#include "esp_heap_caps.h"
#include "log.h"
#include "xclk.h"
#include "board_config.h"

static const char *TAG = "main";

/* ---------- WiFi ---------- */
#define WIFI_SSID     "RCSA3_WIFI"
#define WIFI_PASS     "RCSA12345678"

WebServer server(80);

/* ---------- Utils: SOI/EOI + Hexdump (ถ้าจะใช้) ---------- */
static int find_soi(const uint8_t* buf, size_t len) {
  if (len < 3) return -1;
  for (size_t i = 0; i + 2 < len; ++i) {
    if (buf[i] == 0xFF && buf[i+1] == 0xD8 && buf[i+2] == 0xFF) return (int)i;
  }
  return -1;
}
static int find_eoi(const uint8_t* buf, size_t len) {
  if (len < 2) return -1;
  const uint8_t *p = buf + len - 2;
  while (p >= buf) {
    if (p[0] == 0xFF && p[1] == 0xD9) return (int)(p - buf);
    --p;
  }
  return -1;
}
static void hexdump_all_throttled(const char* tag, const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i += 16) {
    char line[3*16 + 1];
    size_t k = 0;
    for (size_t j = 0; j < 16 && (i + j) < n; ++j) k += sprintf(&line[k], "%02X ", p[i + j]);
    line[k] = '\0';
    SCCB_LOGI(tag, "%04u: %s", (unsigned)i, line);
    if ((i & 0x3FF) == 0) delay(0);
  }
  SCCB_LOGI(tag, "... shown %u bytes", (unsigned)n);
}

/* ---------- พิมพ์สถานะหน่วยความจำ ---------- */
static void print_mem() {
  SCCB_LOGD(TAG,"Free heap=%u, Free PSRAM=%u, Free DMA heap=%u",
            ESP.getFreeHeap(), ESP.getFreePsram(),
            heap_caps_get_free_size(MALLOC_CAP_DMA));
}

/* ---------- (ออปชัน) ตาราง resolution ---------- */
extern const resolution_info_t resolution[];
static const char* k_fs_names[FRAMESIZE_INVALID] = {
  "96X96", "QQVGA", "128X128", "QCIF", "HQVGA", "240X240",
  "QVGA", "320X320", "CIF", "HVGA", "VGA", "SVGA",
  "XGA", "HD", "SXGA", "UXGA",
  "FHD", "P_HD", "P_3MP", "QXGA",
  "QHD", "WQXGA", "P_FHD", "QSXGA", "5MP"
};
static void print_resolution_table() {
  Serial.printf("resolution[] base address = %p\n", (void*)resolution);
  for (int i = 0; i < (int)FRAMESIZE_INVALID; ++i) { (void)k_fs_names[i]; (void)resolution[i]; }
}

/* ---------- MJPEG stream handler ---------- */
static void handle_stream() {
  WiFiClient client = server.client();
  if (!client) return;

  // 0 = ไม่ล็อก FPS; ใส่ 33 -> ~30fps เพื่อลดโหลดก็ได้
  const uint32_t FPS_LIMIT_MS = 0;
  client.setTimeout(3);
  client.setNoDelay(true);

  // ส่วนหัวของ multipart stream
  client.print(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
    "Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n"
    "Pragma: no-cache\r\n"
    "Connection: close\r\n"
    "\r\n"
  );

  uint32_t last = millis();
  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { delay(1); continue; }

    // header ของแต่ละเฟรม
    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %u\r\n\r\n", fb->len);

    // ส่งบัฟเฟอร์ JPEG แบบไหลต่อเนื่อง
    size_t sent = 0;
    while (sent < fb->len) {
      size_t n = client.write(fb->buf + sent, fb->len - sent);
      if (!n) break;  // ไคลเอนต์หยุดรับ
      sent += n;
      delay(0);
    }
    client.print("\r\n");
    esp_camera_fb_return(fb);

    if (sent < fb->len) break;  // ส่งไม่ครบ แปลว่าหลุด

    // ล็อก FPS ถ้าอยากคุมโหลด/แบนด์วิดท์
    if (FPS_LIMIT_MS) {
      int32_t wait_ms = (int32_t)FPS_LIMIT_MS - (int32_t)(millis() - last);
      if (wait_ms > 0) delay(wait_ms);
      last = millis();
    }
  }
}

/* ---------- HTTP Handlers ---------- */
// หน้า HTML ดูสตรีม + สแน็ป
static void handle_root() {
  String html = F(
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32-CAM</title>"
    "<style>body{font-family:sans-serif;margin:20px}"
    "img{max-width:100%;height:auto;border:1px solid #ccc}</style>"
    "</head><body>"
    "<h3>ESP32-CAM Stream (MJPEG)</h3>"
    "<p><a href='/jpg' target='_blank'>Snapshot</a> | "
    "<a href='/hexdump' target='_blank'>Hexdump(256B)</a></p>"
    "<img src='/stream'/>"
    "</body></html>"
  );
  server.send(200, "text/html", html);
}

// ส่งเฟรม JPEG เดี่ยว
static void handle_jpg() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(503, "text/plain", "camera busy");
    return;
  }

  // ปกติไม่ต้อง trim SOI/EOI
  const uint8_t* ptr = fb->buf;
  size_t len = fb->len;

  server.setContentLength(len);
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "image/jpeg", "");   // เฮดเดอร์

  WiFiClient client = server.client();
  size_t off = 0;
  while (off < len) {
    size_t chunk = client.write(ptr + off, len - off);
    if (chunk == 0) break;
    off += chunk;
    delay(0);
  }
  client.flush();

  esp_camera_fb_return(fb);
}

// hexdump 256 ไบต์แรก (ดีบัก)
static void handle_hexdump() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { server.send(503, "text/plain", "camera busy"); return; }
  size_t n = (fb->len < 256) ? fb->len : 256;
  hexdump_all_throttled(TAG, fb->buf, n);
  esp_camera_fb_return(fb);
  server.send(200, "text/plain", "dumped 256 bytes to Serial");
}

/* ------------------- Sketch ------------------- */
void setup() {
  Serial.begin(921600);
  delay(300);

  sccb_log_set_level(SCCB_LOG_DEBUG);

  Serial.println("\n[Dump resolution[]]");
  print_resolution_table();

  Serial.println("\n[TestCamera] Init...");
  Serial.printf("PSRAM: %s\n", psramFound() ? "FOUND" : "NOT FOUND");

  camera_config_t config = {};
  config.ledc_channel   = LEDC_CHANNEL_0;
  config.ledc_timer     = LEDC_TIMER_0;
  config.pin_d0         = Y2_GPIO_NUM;
  config.pin_d1         = Y3_GPIO_NUM;
  config.pin_d2         = Y4_GPIO_NUM;
  config.pin_d3         = Y5_GPIO_NUM;
  config.pin_d4         = Y6_GPIO_NUM;
  config.pin_d5         = Y7_GPIO_NUM;
  config.pin_d6         = Y8_GPIO_NUM;
  config.pin_d7         = Y9_GPIO_NUM;
  config.pin_xclk       = XCLK_GPIO_NUM;
  config.pin_pclk       = PCLK_GPIO_NUM;
  config.pin_vsync      = VSYNC_GPIO_NUM;
  config.pin_href       = HREF_GPIO_NUM;
  config.pin_sccb_sda   = SIOD_GPIO_NUM;
  config.pin_sccb_scl   = SIOC_GPIO_NUM;
  config.pin_pwdn       = PWDN_GPIO_NUM;
  config.pin_reset      = RESET_GPIO_NUM;

  config.xclk_freq_hz   = 20000000;
  config.pixel_format   = PIXFORMAT_JPEG;
  config.frame_size     = FRAMESIZE_HVGA;   // 320x240
  config.jpeg_quality   = 12;               // เลขน้อย = คมชัดกว่า/ไฟล์ใหญ่
  config.fb_count       = 4;                // เพิ่มเป็น 3 เพื่อสตรีมลื่นขึ้น
  config.sccb_i2c_port  = I2C_NUM_1;

  ovcam_err_t err = esp_camera_init(&config);
  if (err != OVCAM_OK) {
    Serial.printf("[TestCamera] Init failed: 0x%x\n", err);
    while (true) delay(1000);
  }

  print_mem();

  sensor_t *s = esp_camera_sensor_get();
  Serial.printf("Sensor PID: 0x%04x\n", s ? s->id.PID : 0);
  Serial.println("[TestCamera] Camera init OK");
  if (s) {
    s->set_brightness(s, 1);
    s->set_saturation(s, -1);
    s->set_contrast(s, 2);
  }

  /* ---------- WiFi connect ---------- */
  Serial.printf("Connecting WiFi SSID=\"%s\" ...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setSleep(false);  // ปิด power save ให้ลิงก์นิ่งสำหรับสตรีม
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print('.');
    if (millis() - t0 > 20000) {
      Serial.println("\nWiFi connect timeout");
      break;
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi not connected; you can still use Serial.");
  }

  /* ---------- HTTP routes ---------- */
  server.on("/", handle_root);
  server.on("/jpg", HTTP_GET, handle_jpg);
  server.on("/hexdump", HTTP_GET, handle_hexdump);
  server.on("/stream", HTTP_GET, handle_stream);   // ✅ สตรีม MJPEG
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  delay(1);
}
