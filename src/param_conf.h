#pragma once

// ===== UDP target =====
#define UDP_REMOTE_IP   "192.168.1.145"
#define UDP_REMOTE_PORT 5005
#define RX_PORT         5009
#define UDP_DATA_CHUNK  1400   

// ===== PACKET  =====
#define ID_PACKET       0
#define CMD_PACKET      0x01
#define print_debug_packet     1

// ===== mini head =====
#define MODE            1
#define CHUNK           2
#define TOTAL_CHUNK     2
#define FRAME_SIDE      1
#define RESERVED        2

// ===== CRC32 =====
#define CRC             1
#define CRC32_FAST      1


//=====type board====
// #define CONFIG_IDF_TARGET_ESP32    1
// #define CONFIG_IDF_TARGET_ESP32S2  1
#define CONFIG_IDF_TARGET_ESP32S3  1


//====DMA PSRAM Mode====
#define CONFIG_CAMERA_PSRAM_DMA 1