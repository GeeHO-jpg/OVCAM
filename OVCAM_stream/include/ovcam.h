#pragma once

#if defined(I2Ctest)


#include "sccb.h"
#include "log.h"
#include "board_config.h"

#if defined(OV2640)
  #include "OV2640_reg.h"
#endif

#elif defined(XCLKtest)

  #include "board_config.h"  // XCLK_GPIO_NUM
  #include "esp_camera.h"      // camera_config_t
  #include "xclk.h"           // camera_enable_out_clock()

#else
  #include <esp_camera.h>
  #include "esp32-hal-psram.h"
  #include "esp_heap_caps.h"
  #include "../lib/core/log.h"
  #include "../lib/hal/xclk.h"
#endif
