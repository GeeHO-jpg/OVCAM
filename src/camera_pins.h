

#pragma once

// เลือกพินตามรุ่นกล้อง (ต้องนิยาม CAMERA_MODEL_* มาจาก board_config.h)
// ใส่แค่ “หนึ่งรุ่น” เท่านั้น



/* เพิ่มรุ่นอื่น ๆ ต่อด้วย #elif defined(CAMERA_MODEL_...) ตามต้องการ */
// WROVER-KIT PIN Map
#if defined(CAMERA_MODEL_WROVER_KIT)

  #define CAM_PIN_PWDN   14
  #define CAM_PIN_RESET  32
  #define CAM_PIN_XCLK   21
  #define CAM_PIN_SIOD   26
  #define CAM_PIN_SIOC   27
  #define CAM_PIN_VSYNC  25
  #define CAM_PIN_HREF   23
  #define CAM_PIN_PCLK   22

  // Y2..Y9 → D0..D7
  #define CAM_PIN_D0      4   // Y2
  #define CAM_PIN_D1      5   // Y3
  #define CAM_PIN_D2     18   // Y4
  #define CAM_PIN_D3     19   // Y5
  #define CAM_PIN_D4     36   // Y6
  #define CAM_PIN_D5     39   // Y7
  #define CAM_PIN_D6     34   // Y8
  #define CAM_PIN_D7     33   // Y9

#elif defined(CAMERA_MODEL_AI_THINKER)

  #define CAM_PIN_PWDN   32
  #define CAM_PIN_RESET  -1
  #define CAM_PIN_XCLK    0
  #define CAM_PIN_SIOD   26
  #define CAM_PIN_SIOC   27
  #define CAM_PIN_VSYNC  25
  #define CAM_PIN_HREF   23
  #define CAM_PIN_PCLK   22

  // Y2..Y9 → D0..D7
  #define CAM_PIN_D0      5   // Y2
  #define CAM_PIN_D1     18   // Y3
  #define CAM_PIN_D2     19   // Y4
  #define CAM_PIN_D3     21   // Y5
  #define CAM_PIN_D4     36   // Y6
  #define CAM_PIN_D5     39   // Y7
  #define CAM_PIN_D6     34   // Y8
  #define CAM_PIN_D7     35   // Y9

// ESP32S3 (WROOM) PIN Map
#elif defined(BOARD_ESP32S3_WROOM)
  #define CAM_PIN_PWDN 38
  #define CAM_PIN_RESET -1
  #define CAM_PIN_VSYNC 6
  #define CAM_PIN_HREF 7
  #define CAM_PIN_PCLK 13
  #define CAM_PIN_XCLK 15
  #define CAM_PIN_SIOD 4
  #define CAM_PIN_SIOC 5
  #define CAM_PIN_D0 11
  #define CAM_PIN_D1 9
  #define CAM_PIN_D2 8
  #define CAM_PIN_D3 10
  #define CAM_PIN_D4 12
  #define CAM_PIN_D5 18
  #define CAM_PIN_D6 17
  #define CAM_PIN_D7 16

// ESP32S3 (GOOUU TECH)
#elif defined(BOARD_ESP32S3_GOOUUU)
  #define CAM_PIN_PWDN -1
  #define CAM_PIN_RESET -1
  #define CAM_PIN_VSYNC 6
  #define CAM_PIN_HREF 7
  #define CAM_PIN_PCLK 13
  #define CAM_PIN_XCLK 15
  #define CAM_PIN_SIOD 4
  #define CAM_PIN_SIOC 5
  #define CAM_PIN_D0 11
  #define CAM_PIN_D1 9
  #define CAM_PIN_D2 8
  #define CAM_PIN_D3 10
  #define CAM_PIN_D4 12
  #define CAM_PIN_D5 18
  #define CAM_PIN_D6 17
  #define CAM_PIN_D7 16

#elif defined(CAMERA_MODEL_ESP32S3_EYE)
  #define CAM_PIN_PWDN   -1
  #define CAM_PIN_RESET  -1
  #define CAM_PIN_XCLK   15
  #define CAM_PIN_SIOD   4
  #define CAM_PIN_SIOC   5
  #define CAM_PIN_VSYNC  6
  #define CAM_PIN_HREF   7
  #define CAM_PIN_PCLK   13

  // Y2..Y9 → D0..D7
  #define CAM_PIN_D0     11   // Y2
  #define CAM_PIN_D1      9   // Y3
  #define CAM_PIN_D2      8   // Y4
  #define CAM_PIN_D3     10   // Y5
  #define CAM_PIN_D4     12   // Y6
  #define CAM_PIN_D5     18  // Y7
  #define CAM_PIN_D6     17   // Y8
  #define CAM_PIN_D7     16   // Y9
#else
  #error "กรุณาเลือก CAMERA_MODEL_* ใน board_config.h (กำหนดให้เหลือรุ่นเดียว)"
#endif
