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
//system
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp32-hal-psram.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <lwip/ip4_addr.h>

//handmade
#include "esp_camera.h"
#include "log.h"
#include "xclk.h"
#include "board_config.h"
#include "udp.h"
#include "UDPCommands.h"
#include "UDPPacket.h"
#include "UDPPacketHeader.h"
#include "crc32.h"
#include "task.h"
#include "param_conf.h"




#endif
