#include <Arduino.h>
#include "esp_camera.h"
#include "esp32-hal-psram.h"
#include "esp_heap_caps.h"
#include "log.h"
#include "xclk.h"
#include "board_config.h"

static const char *TAG = "main";

/* ---------- Utils: SOI/EOI + Hexdump ---------- */
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

// hexdump ทั้งเฟรมแบบ throttle เพื่อลดการบล็อก Serial
static void hexdump_all_throttled(const char* tag, const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i += 16) {
    char line[3*16 + 1];
    size_t k = 0;
    for (size_t j = 0; j < 16 && (i + j) < n; ++j) {
      k += sprintf(&line[k], "%02X ", p[i + j]);
    }
    line[k] = '\0';
    SCCB_LOGI(tag, "%04u: %s", (unsigned)i, line);

    // throttle ทุก ๆ ~1KB ที่พิมพ์ เพื่อไม่ให้บล็อกยาวเกิน
    if ( (i & 0x3FF) == 0 ) delay(0);
  }
  SCCB_LOGI(tag, "... shown %u bytes", (unsigned)n);
}

/* ---------- แค่ช่วยพิมพ์สถานะหน่วยความจำ ---------- */
static void print_mem() {
  SCCB_LOGD(TAG,"Free heap=%u, Free PSRAM=%u, Free DMA heap=%u",
            ESP.getFreeHeap(), ESP.getFreePsram(),
            heap_caps_get_free_size(MALLOC_CAP_DMA));
}

/* ---------- (ออปชัน) ตาราง resolution ---------- */
extern const resolution_info_t resolution[]; // มีจากโปรเจกต์คุณอยู่แล้ว
static const char* k_fs_names[FRAMESIZE_INVALID] = {
  "96X96", "QQVGA", "128X128", "QCIF", "HQVGA", "240X240",
  "QVGA", "320X320", "CIF", "HVGA", "VGA", "SVGA",
  "XGA", "HD", "SXGA", "UXGA",
  /* 3MP */ "FHD", "P_HD", "P_3MP", "QXGA",
  /* 5MP */ "QHD", "WQXGA", "P_FHD", "QSXGA", "5MP"
};
static void print_resolution_table() {
  Serial.printf("resolution[] base address = %p\n", (void*)resolution);
  for (int i = 0; i < (int)FRAMESIZE_INVALID; ++i) {
    (void)k_fs_names[i]; (void)resolution[i];
    // เปิดพิมพ์เพิ่มได้ตามต้องการ
  }
}

/* ------------------- Sketch ------------------- */
void setup() {
  // เร่งบอดเรตเพื่อพิมพ์ได้เร็วขึ้น (ถ้าชิป USB-UART รองรับ)
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
  config.frame_size     = FRAMESIZE_QVGA;
  config.jpeg_quality   = 12;
  config.fb_count       = 2;
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
    s->set_framesize(s, FRAMESIZE_QVGA);
    s->set_brightness(s, 1);
    s->set_saturation(s, -1);
    s->set_contrast(s, 2);
  }

  // ดึงเฟรม 1 ครั้ง
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    SCCB_LOGE(TAG, "fb_get failed");
    return;
  }

  Serial.printf("Got frame %dx%d len=%u\n", fb->width, fb->height, fb->len);

  // ตัดให้พอดี JPEG [SOI..EOI]
  int soi = find_soi(fb->buf, fb->len);
  int eoi = find_eoi(fb->buf, fb->len);
  size_t valid_len = fb->len;
  size_t head_off  = 0;

  if (soi >= 0 && eoi >= 0 && eoi > soi) {
    head_off  = (size_t)soi;
    valid_len = (size_t)(eoi - soi + 2); // รวม EOI
  } else {
    SCCB_LOGW(TAG, "trim fail (SOI=%d EOI=%d), use raw len", soi, eoi);
  }

  // ก็อปปี้ออกมาไว้เอง (PSRAM ถ้ามี)
  uint8_t *copy = (uint8_t*)heap_caps_malloc(
      valid_len,
      MALLOC_CAP_8BIT | (psramFound() ? MALLOC_CAP_SPIRAM : 0));
  if (!copy) {
    SCCB_LOGE(TAG, "copy alloc failed (len=%u)", (unsigned)valid_len);
    esp_camera_fb_return(fb);
    esp_camera_deinit();
    return;
  }
  memcpy(copy, fb->buf + head_off, valid_len);

  // คืน fb แล้วปิดกล้องจริง ๆ
  esp_camera_fb_return(fb);
  esp_camera_deinit();

  SCCB_LOGI(TAG, "JPEG ready: ptr=%p len=%u (SOI=%d EOI=%d off=%u)",
            (void*)copy, (unsigned)valid_len, soi, eoi, (unsigned)head_off);

  // hexdump ทั้งเฟรม (ระวังว่าจะยาวมาก)
  hexdump_all_throttled(TAG, copy, valid_len);

  free(copy);
}

void loop() {
  // one-shot; ไม่ทำอะไรต่อ
  delay(1000);
}
