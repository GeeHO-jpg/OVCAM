#include <Arduino.h>

#define XCLKtest
#include <ovcam.h>


#if defined(ESP_PLATFORM) || (defined(ARDUINO) && defined(ARDUINO_ARCH_ESP32))
  #include "driver/ledc.h"      // LEDC_TIMER_0 / LEDC_CHANNEL_0
#endif

#define XCLK_FREQ_HZ  20000000  // 20 MHz

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println();

  camera_config_t config = {0};
  config.ledc_timer   = LEDC_TIMER_0;     // เลือก timer ว่าง
  config.ledc_channel = LEDC_CHANNEL_0;   // เลือก channel ว่าง
  config.pin_xclk     = XCLK_GPIO_NUM;    // << ต้องเป็น XCLK (ไม่ใช่ PCLK)
  config.xclk_freq_hz = XCLK_FREQ_HZ;

  ovcam_err_t err = camera_enable_out_clock(&config);
  if (err != HAL_OK) {
    Serial.printf("[XCLK] enable failed: 0x%x\n", err);
  } else {
    Serial.printf("[XCLK] ON @ %d Hz (pin %d)\n", XCLK_FREQ_HZ, XCLK_GPIO_NUM);
  }
}

void loop() {
  // ว่าง ๆ พอ (แค่เปิด XCLK)
}
