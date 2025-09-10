#pragma once

// เลือกพินตามรุ่นกล้อง (ต้องนิยาม CAMERA_MODEL_* มาจาก board_config.h)
// ใส่แค่ “หนึ่งรุ่น” เท่านั้น

#if defined(CAMERA_MODEL_WROVER_KIT)

  #define XCLK_GPIO_NUM    21
  #define PWDN_GPIO_NUM    -1
  #define RESET_GPIO_NUM   -1
  #define SIOD_GPIO_NUM    26
  #define SIOC_GPIO_NUM    27
  #define Y9_GPIO_NUM      33
  #define Y8_GPIO_NUM      34
  #define Y7_GPIO_NUM      39
  #define Y6_GPIO_NUM      36
  #define Y5_GPIO_NUM      19
  #define Y4_GPIO_NUM      18
  #define Y3_GPIO_NUM       5
  #define Y2_GPIO_NUM       4
  #define VSYNC_GPIO_NUM   25
  #define HREF_GPIO_NUM    23
  #define PCLK_GPIO_NUM    22

#elif defined(CAMERA_MODEL_AI_THINKER)

  #define PWDN_GPIO_NUM    32
  #define RESET_GPIO_NUM   -1
  #define XCLK_GPIO_NUM     0
  #define SIOD_GPIO_NUM    26
  #define SIOC_GPIO_NUM    27
  #define Y9_GPIO_NUM      35
  #define Y8_GPIO_NUM      34
  #define Y7_GPIO_NUM      39
  #define Y6_GPIO_NUM      36
  #define Y5_GPIO_NUM      21
  #define Y4_GPIO_NUM      19
  #define Y3_GPIO_NUM      18
  #define Y2_GPIO_NUM       5
  #define VSYNC_GPIO_NUM   25
  #define HREF_GPIO_NUM    23
  #define PCLK_GPIO_NUM    22

/* เพิ่มรุ่นอื่น ๆ ต่อด้วย #elif defined(CAMERA_MODEL_...) ตามต้องการ */

#else
  #error "กรุณาเลือก CAMERA_MODEL_* ใน board_config.h (กำหนดให้เหลือรุ่นเดียว)"
#endif
