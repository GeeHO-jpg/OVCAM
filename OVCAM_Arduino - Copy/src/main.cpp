

#include <Arduino.h>

#include <OVCAM.h>


// ===== WiFi =====
#define WIFI_SSID "RCSA3_WIFI"
#define WIFI_PASS "RCSA12345678"


IPAddress local_IP(192, 168, 1, 235);     // IP ที่ต้องการ fix
IPAddress gateway(192, 168, 1, 1);       // Gateway ของ Router
IPAddress subnet(255, 255, 255, 0);      // Subnet mask
IPAddress primaryDNS(8, 8, 8, 8);        // DNS (เลือกใส่ก็ได้)
IPAddress secondaryDNS(8, 8, 4, 4);




const char*TAG ="main";
// static const size_t   buff_size   = UDP_DATA_CHUNK;
// static uint8_t        buff[buff_size];




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



// ---------- ปรับตั้งกล้อง ----------
camera_config_t camera_config = {
  .pin_pwdn     = CAM_PIN_PWDN,
  .pin_reset    = CAM_PIN_RESET,
  .pin_xclk     = CAM_PIN_XCLK,
  .pin_sccb_sda = CAM_PIN_SIOD,
  .pin_sccb_scl = CAM_PIN_SIOC,

  .pin_d7   = CAM_PIN_D7, .pin_d6   = CAM_PIN_D6, .pin_d5   = CAM_PIN_D5,
  .pin_d4   = CAM_PIN_D4, .pin_d3   = CAM_PIN_D3, .pin_d2   = CAM_PIN_D2,
  .pin_d1   = CAM_PIN_D1, .pin_d0   = CAM_PIN_D0,
  .pin_vsync= CAM_PIN_VSYNC, .pin_href = CAM_PIN_HREF, .pin_pclk = CAM_PIN_PCLK,

  .xclk_freq_hz = 20000000,
  .ledc_timer   = LEDC_TIMER_0,
  .ledc_channel = LEDC_CHANNEL_0,

  .pixel_format = PIXFORMAT_JPEG,
  .frame_size   = FRAMESIZE_HVGA, // 720pแนวนอน FRAMESIZE_HD,600FRAMESIZE_VGA
  .jpeg_quality = 15,                  // << เริ่มสูงขึ้นเพื่อลด bytes/เฟรม
  .fb_count     = 3,
  .fb_location  = CAMERA_FB_IN_PSRAM,
  .grab_mode    = CAMERA_GRAB_LATEST,  // << เปลี่ยนเป็น LATEST
  .sccb_i2c_port= I2C_NUM_1,
  
};

static void tune_wifi() {
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11B);
  esp_wifi_set_max_tx_power(84); // ~21 dBm
}

static void wifi_connect() {
  WiFi.mode(WIFI_STA);
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    printf("STA Failed to configure\n");
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(250);
    printf(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    printf("[I] WiFi IP: %s\n", WiFi.localIP().toString().c_str());
    tune_wifi(); // << สำคัญ: เรียกหลังเชื่อม
  } else {
    printf("[E] WiFi connect failed\n");
  }
}

// static void initializeBuffer() {
//   for (size_t i = 0; i < buff_size; ++i) {
//     buff[i] = (uint8_t)(i & 0xFF);
//   }
// }


// ---------- Tasks ----------



void setup() {
  Serial.begin(115200);
  WiFi.setSleep(false);
  sccb_log_set_level(SCCB_LOG_INFO);   // << ลด log

  wifi_connect();
  if (!c_udp_open(UDP_REMOTE_IP, UDP_REMOTE_PORT)) { printf("udp_open failed\n"); for(;;){} }

  print_resolution_table();

 

  if (esp_camera_init(&camera_config) != OVCAM_OK) {
    printf("[E] Camera init failed\n"); while(1) delay(1000);
  }
  rots_init();
}

void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
