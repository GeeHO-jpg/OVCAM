/*

                              XCLK


*/


#include <Arduino.h>
#include <esp_camera.h>
#include "esp32-hal-psram.h"
#include "esp_heap_caps.h"
#include "log.h"  // <<<
#include "xclk.h"

#define BOARD_WROVER_KIT
// #define BOARD_ESP32CAM_AITHINKER
// #define BOARD_CAMERA_MODEL_ESP32S2
// #define BOARD_CAMERA_MODEL_ESP32_S3_EYE

#if defined(BOARD_WROVER_KIT)
  #define PWDN_GPIO_NUM 14
  #define RESET_GPIO_NUM 32
  #define XCLK_GPIO_NUM 21
  #define SIOD_GPIO_NUM 26
  #define SIOC_GPIO_NUM 27
  #define Y9_GPIO_NUM 33
  #define Y8_GPIO_NUM 34
  #define Y7_GPIO_NUM 39
  #define Y6_GPIO_NUM 36
  #define Y5_GPIO_NUM 19
  #define Y4_GPIO_NUM 18
  #define Y3_GPIO_NUM 5
  #define Y2_GPIO_NUM 4
  #define VSYNC_GPIO_NUM 25
  #define HREF_GPIO_NUM 23
  #define PCLK_GPIO_NUM 22
#elif defined(BOARD_ESP32CAM_AITHINKER)
  #define PWDN_GPIO_NUM 32
  #define RESET_GPIO_NUM -1
  #define XCLK_GPIO_NUM 0
  #define SIOD_GPIO_NUM 26
  #define SIOC_GPIO_NUM 27
  #define Y9_GPIO_NUM 35
  #define Y8_GPIO_NUM 34
  #define Y7_GPIO_NUM 39
  #define Y6_GPIO_NUM 36
  #define Y5_GPIO_NUM 21
  #define Y4_GPIO_NUM 19
  #define Y3_GPIO_NUM 18
  #define Y2_GPIO_NUM 5
  #define VSYNC_GPIO_NUM 25
  #define HREF_GPIO_NUM 23
  #define PCLK_GPIO_NUM 22
#elif defined(BOARD_CAMERA_MODEL_ESP32S2) || defined(BOARD_CAMERA_MODEL_ESP32_S3_EYE)
  #error "ตอนนี้คอมไพล์ env:esp32dev อยู่ ให้ใช้บอร์ด ESP32 ปกติเท่านั้น"
#else
  #error "กรุณา define บอร์ด 1 รุ่น (เช่น BOARD_WROVER_KIT)"
#endif

static void print_mem() {
  Serial.printf("Free heap=%u, Free PSRAM=%u, Free DMA heap=%u\n",
                ESP.getFreeHeap(), ESP.getFreePsram(),
                heap_caps_get_free_size(MALLOC_CAP_DMA));
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // <<< ตั้งระดับ log ให้เห็น SCCB
  sccb_log_set_level(SCCB_LOG_DEBUG);


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

  ovcam_err_t err = camera_enable_out_clock(&config);
  if (err != OVCAM_OK) {
    Serial.printf("[Test CLK] Init failed: 0x%x\n", err);
    while (true) delay(1000);
  }else{
    Serial.print("clk running");
  }

}

void loop() {
  delay(1000);
}
