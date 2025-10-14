#include "task.h"


static QueueHandle_t s_fbq;    // latest-only queue
static SemaphoreHandle_t s_cam_mtx = NULL;
static volatile bool g_pause_capture = false;
extern camera_config_t camera_config; 

static volatile bool g_in_fb_get  = false;   // capture เข้าช่วง esp_camera_fb_get()
static volatile bool g_tx_sending = false;   // tx_task กำลังแตก chunk/ส่ง


static inline bool is_jpeg(pixformat_t pf) {
    return (pf == PIXFORMAT_JPEG);
}

static bool crosses_jpeg(pixformat_t cur, pixformat_t next) {
    return is_jpeg(cur) ^ is_jpeg(next);
}

typedef struct {
  UDPPacketHeader hdr;
  const uint8_t* payload; 
  uint16_t       payload_len;
}command_t;

command_t command;

static inline int clampi(int v, int lo, int hi){ if(v<lo) return lo; if(v>hi) return hi; return v; }

static bool rd_u8(const command_t* c, uint8_t* out){
  if(!c || c->payload_len < 1) return false; *out = c->payload[0]; return true;
}
static bool rd_i8(const command_t* c, int8_t* out){
  if(!c || c->payload_len < 1) return false; *out = (int8_t)c->payload[0]; return true;
}
static bool rd_u16le(const command_t* c, uint16_t* out){
  if(!c || c->payload_len < 2) return false; *out = (uint16_t)(c->payload[0] | (c->payload[1]<<8)); return true;
}
// extern SemaphoreHandle_t s_cam_mtx;

// static bool apply_framesize_from_id(uint16_t id) {
//   if (id > FRAMESIZE_5MP) return false;           // กันค่านอกช่วง
//   sensor_t* s = esp_camera_sensor_get(); if (!s) return false;
//   xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
//   int r = s->set_framesize(s, (framesize_t)id);
//   xSemaphoreGive(s_cam_mtx);
//   if (r == 0) camera_config.frame_size = (framesize_t)id;   // sync config
//   return r == 0;
// }

static inline void flush_frame_queue() {
  camera_fb_t* old = NULL;
  while (xQueueReceive(s_fbq, &old, 0) == pdTRUE) {
    esp_camera_fb_return(old);
  }
}

static bool safe_reinit(pixformat_t next_pf, framesize_t next_fs) {
  // 1) pause capture ก่อน
  g_pause_capture = true;
  vTaskDelay(pdMS_TO_TICKS(2));

  // 2) รอให้พ้นช่วง fb_get และหยุดส่ง
  uint32_t t0 = millis();
  while ((g_in_fb_get || g_tx_sending) && (millis() - t0 < 100)) {
    vTaskDelay(1);
  }

  // 3) ล้างเฟรมค้างในคิว
  flush_frame_queue();

  // 4) deinit/init ภายใต้ mutex
  bool ok = true;
  xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
  esp_camera_deinit();
  camera_config.pixel_format = next_pf;
  camera_config.frame_size   = next_fs;
  ok = (esp_camera_init(&camera_config) == ESP_OK);
  xSemaphoreGive(s_cam_mtx);

  // 5) resume
  g_pause_capture = false;
  return ok;
}

static bool apply_framesize_from_id(uint16_t id) {
  if (id > FRAMESIZE_5MP) return false;
  sensor_t* s = esp_camera_sensor_get();
  if (!s) return false;

  // อยู่ใน JPEG เหมือนเดิม → hot-change
  xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
  int r = s->set_framesize(s, (framesize_t)id);
  xSemaphoreGive(s_cam_mtx);

  if (r == 0) {
    camera_config.frame_size = (framesize_t)id;
    return true;
  }

  // // set ไม่ผ่าน → re-init (ไม่ต้อง pause ลูป)
  // xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
  // esp_camera_deinit();
  // camera_config.frame_size = (framesize_t)id;
  // bool ok = (esp_camera_init(&camera_config) == ESP_OK);
  // xSemaphoreGive(s_cam_mtx);

  return safe_reinit(camera_config.pixel_format, (framesize_t)id);
}

static bool apply_pixformat_from_id(uint16_t id) {
  if (id > PIXFORMAT_RGB555) return false;
  sensor_t* s = esp_camera_sensor_get(); 
  if (!s) return false;

  pixformat_t next_pf = (pixformat_t)id;

  // เฉพาะกรณี "ข้าม JPEG" เท่านั้นที่ต้อง pause + deinit/init
  if (crosses_jpeg(camera_config.pixel_format, next_pf)) {
    return safe_reinit(next_pf, camera_config.frame_size);
  }

  // ไม่ข้าม JPEG → ลอง hot-change "โดยไม่ต้อง pause"
  bool ok = false;
  xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
  int r = s->set_pixformat ? s->set_pixformat(s, next_pf) : -1;
  xSemaphoreGive(s_cam_mtx);

  if (r == 0) {
    camera_config.pixel_format = next_pf;
    ok = true;
  } else {
    // hot-change ล้มเหลว → re-init (ยังคง 'ไม่' จำเป็นต้อง pause ทั้งระบบ)
    return safe_reinit(next_pf, camera_config.frame_size);
  }
  return ok;
}

static bool apply_param_by_id(const command_t* c){
  if(!c) return false;
  sensor_t* s = esp_camera_sensor_get();
  if(!s) return false;

  uint16_t id = c->hdr.id;
  bool ok = false;

  switch(id){
    case 0: { // framesize_t (u8)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      return apply_framesize_from_id((uint16_t)v);  // ใช้ฟังก์ชันเดิมของคุณ (hot-change -> reinit ถ้าจำเป็น)
    }
    case 1: { // scale (bool) - ไม่มี API มาตรฐานบน OV2640; เก็บไว้เฉยๆ/ข้าม
      uint8_t v; if(!rd_u8(c,&v)) return false;
      // TODO: ถ้าคุณผูก scale กับอย่างอื่น ให้เรียกตรงนี้
      return true; // รับคำสั่งไว้เฉยๆ
    }
    case 2: { // binning (bool) - ส่วนใหญ่ไม่มีบน OV2640
      uint8_t v; if(!rd_u8(c,&v)) return false;
      // TODO: ถ้าเซ็นเซอร์คุณรองรับ ให้แมปไป API ที่มี
      return true;
    }
    case 3: { // quality (0..63)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      v = (uint8_t)clampi(v,0,63);
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_quality ? (s->set_quality(s, v)==0) : false);
      camera_config.jpeg_quality = v;
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 4: { // brightness (-2..2)
      int8_t v; if(!rd_i8(c,&v)) return false;
      v = (int8_t)clampi(v,-2,2);
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_brightness ? (s->set_brightness(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 5: { // contrast (-2..2)
      int8_t v; if(!rd_i8(c,&v)) return false;
      v = (int8_t)clampi(v,-2,2);
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_contrast ? (s->set_contrast(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 6: { // saturation (-2..2)
      int8_t v; if(!rd_i8(c,&v)) return false;
      v = (int8_t)clampi(v,-2,2);
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_saturation ? (s->set_saturation(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 7: { // sharpness (-2..2) *บางเซ็นเซอร์ไม่มี
      int8_t v; if(!rd_i8(c,&v)) return false;
      v = (int8_t)clampi(v,-2,2);
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_sharpness ? (s->set_sharpness(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 8: { // denoise (u8) *มีในบางรุ่น
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_denoise ? (s->set_denoise(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 9: { // special_effect (0..6)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      v = (uint8_t)clampi(v,0,6);
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_special_effect ? (s->set_special_effect(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 10: { // wb_mode (0..4)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      v = (uint8_t)clampi(v,0,4);
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_wb_mode ? (s->set_wb_mode(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 11: { // awb (0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_whitebal ? (s->set_whitebal(s, v) == 0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 12: { // awb_gain (0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_awb_gain ? (s->set_awb_gain(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 13: { // aec (exposure ctrl 0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_exposure_ctrl ? (s->set_exposure_ctrl(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 14: { // aec2 (0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_aec2 ? (s->set_aec2(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 15: { // ae_level (-2..2)
      int8_t v; if(!rd_i8(c,&v)) return false;
      v = (int8_t)clampi(v,-2,2);
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_ae_level ? (s->set_ae_level(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 16: { // aec_value (0..1200) u16
      uint16_t v; if(!rd_u16le(c,&v)) return false;
      v = (uint16_t)clampi(v,0,1200);
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_aec_value ? (s->set_aec_value(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 17: { // agc (0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_gain_ctrl ? (s->set_gain_ctrl(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 18: { // agc_gain (0..30)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      v = (uint8_t)clampi(v,0,30);
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_agc_gain ? (s->set_agc_gain(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 19: { // gainceiling (0..6)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      v = (uint8_t)clampi(v,0,6);
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_gainceiling ? (s->set_gainceiling(s, (gainceiling_t)v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 20: { // bpc (0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_bpc ? (s->set_bpc(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 21: { // wpc (0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_wpc ? (s->set_wpc(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 22: { // raw_gma (0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_raw_gma ? (s->set_raw_gma(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 23: { // lenc (0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_lenc ? (s->set_lenc(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 24: { // hmirror (0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_hmirror ? (s->set_hmirror(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 25: { // vflip (0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_vflip ? (s->set_vflip(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 26: { // dcw (0/1) downsize enable
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_dcw ? (s->set_dcw(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    case 27: { // colorbar (0/1)
      uint8_t v; if(!rd_u8(c,&v)) return false;
      xSemaphoreTake(s_cam_mtx, portMAX_DELAY);
      ok = (s->set_colorbar ? (s->set_colorbar(s, v)==0) : false);
      xSemaphoreGive(s_cam_mtx);
      return ok;
    }
    default:
      return false;
  }
}

static bool do_flip(const command_t* c){

  if(!c) return false;
  sensor_t* s = esp_camera_sensor_get();
  if(!s) return false;

  g_pause_capture = true;
  vTaskDelay(pdMS_TO_TICKS(1));
  xSemaphoreTake(s_cam_mtx, portMAX_DELAY);

  uint16_t cmd = c->hdr.cmd;

  int cur = 0;
  int r   = -1;

  switch (cmd)
  {
  case 0x53:
    /* code */
    if(&s->status) cur = s->status.vflip;
    r = s->set_vflip?s->set_vflip(s,cur? 0:1):-1;
    break;
  case 0x54:
    /* code */
    if(&s->status) cur = s->status.hmirror;
    r = s->set_hmirror?s->set_hmirror(s,cur? 0:1):-1;
    break;
  
  default:
    break;
  }
  xSemaphoreGive(s_cam_mtx);
  g_pause_capture = false;
  return r == 0;
}

static void handle_command(const command_t* c) {
  switch (c->hdr.cmd) {
    case 0x50: /* CMD_FS */ {
      bool ok = apply_framesize_from_id(c->hdr.id);
      printf("[CMD_FS] id=%u -> %s\n", c->hdr.id, ok ? "OK" : "FAIL");
      break;
    }
    case 0x51: /* CMD_PF */ {
      bool ok = apply_pixformat_from_id(c->hdr.id);
      printf("[CMD_PF] id=%u -> %s\n", c->hdr.id, ok ? "OK" : "FAIL");
      break;
    }
    case 0x52: /* CMD_SET */ {
      bool ok = apply_param_by_id(c);
      printf("[CMD_SET] id=%u -> %s\n", c->hdr.id, ok ? "OK" : "FAIL");
      break;
    }
    case 0x53: /* CMD_v_flip */ {
      bool ok = do_flip(c);
      printf("[CMD_V_FLIP] id=%u -> %s\n", c->hdr.id, ok ? "OK" : "FAIL");
      break;
    }
    case 0x54: /* CMD_H_flip */ {
      bool ok = do_flip(c);
      printf("[CMD_H_FLIP] id=%u -> %s\n", c->hdr.id, ok ? "OK" : "FAIL");
      break;
    }

    default:
      break;
  }
}

static bool on_rx_packet(const uint8_t* p, int n, uint32_t sip, uint16_t sport) {
  if (n < (int)UDPPACKETHEADER_SIZE) return false;

  const uint16_t payload_len = (uint16_t)(p[7] | (p[8] << 8));
  const int need_no_crc   = (int)UDPPACKETHEADER_SIZE + (int)payload_len; // header+payload
  const int need_with_crc = need_no_crc + 4;                               // +CRC32

  if (n < need_no_crc) return false;          // ยังไม่ครบ payload → ไม่ผ่าน
  if (n >= need_with_crc) {                   // มี CRC → ตรวจเพิ่ม
    const uint32_t crc_cal  = crc32_calc(p, (size_t)need_no_crc); // header+payload
    const uint8_t* crc_ptr  = p + need_no_crc;
    const uint32_t crc_recv =  (uint32_t)crc_ptr[0]
                             | ((uint32_t)crc_ptr[1] << 8)
                             | ((uint32_t)crc_ptr[2] << 16)
                             | ((uint32_t)crc_ptr[3] << 24);
    return crc_cal == crc_recv;
  }
  return true;                                 // พอดี header+payload → ไม่มี CRC ก็ถือว่าผ่าน
}


static bool extract_pkt(command_t* out,const uint8_t* pkt, int n) {
  if (!pkt || n < (int)UDPPACKETHEADER_SIZE) return false;

  // ใช้ฟังก์ชันเดิมของคุณ (malloc ข้างใน) แล้ว copy ออกมาเป็นค่า
  UDPPacketHeader* h = GetUDPPacketHeader((uint8_t*)pkt, (size_t)n);
  if (!h) return false;
  out->hdr = *h;   // copy by value
  free(h);

  // เช็กว่ามีพอสำหรับ payload (ไม่สน CRC)
  int need = (int)UDPPACKETHEADER_SIZE + (int)out->hdr.payload_size;
  if (n < need) return false;

  // ชี้ payload แบบ zero-copy
  out->payload     = pkt + UDPPACKETHEADER_SIZE;
  out->payload_len = out->hdr.payload_size;


  // // log สั้น ๆ
  // printf("sig=%.4s id=%u cmd=0x%02X payload=%u\n",
  //        out->hdr.signature,
  //        (unsigned)out->hdr.id,
  //        out->hdr.cmd,
  //        (unsigned)out->hdr.payload_size);

  // // พิมพ์ payload (hex)
  // for (int i = 0; i < out->payload_len; ++i) {
  //   printf("%02X ", out->payload[i]);
  //   if ((i + 1) % 16 == 0) printf("\n");
  // }
  // if (out->payload_len % 16) printf("\n");

  return true;
}

void rots_init(){
  s_fbq = xQueueCreate(1, sizeof(camera_fb_t*)); // latest-only
  s_cam_mtx = xSemaphoreCreateMutex(); 

  // pin ไป core 1 เพื่อลดชนกับ Wi-Fi
  xTaskCreatePinnedToCore(capture_task, "cap", 8192, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(tx_task,      "tx",  8192, NULL, 5, NULL, 1);
  xTaskCreatePinnedToCore(rx_task,      "rx",  4096, NULL, 3, NULL, 1);
}


void capture_task(void*){
  sensor_t *s = esp_camera_sensor_get();
  if (s) s->set_quality(s, camera_config.jpeg_quality);

  for(;;){
    if (g_pause_capture) { vTaskDelay(1); continue; }

    g_in_fb_get = true;
    camera_fb_t *fb = esp_camera_fb_get();   // ไม่ต้องล็อก s_cam_mtx
    g_in_fb_get = false;

    if (!fb) { vTaskDelay(1); continue; }

    camera_fb_t* old = NULL;
    if (xQueueReceive(s_fbq, &old, 0) == pdTRUE) {
      esp_camera_fb_return(old);            // ไม่ต้องล็อก s_cam_mtx
    }
    xQueueSend(s_fbq, &fb, 0);
  }
}

void tx_task(void*){
  static uint32_t frame_idx = 0;
  static bool printed_first_frame = false;
  const uint16_t  MAX_CHUNK = UDP_DATA_CHUNK;    
  const uint16_t  MINI_HDR  = MODE+CHUNK+TOTAL_CHUNK+FRAME_SIDE+RESERVED;  
  static uint8_t txbuf[UDPPACKETHEADER_SIZE + UDP_DATA_CHUNK  + (CRC ? 4 : 0)];


  //overheard
  UDPPacketHeader * overhead = CreateUDPPacketHeader(ID_PACKET, CMD_PACKET, UDP_DATA_CHUNK);
  //packet
  UDPPacket *pack = CreateUDPPacket(overhead);
  if(!pack)
  {
    printf("❌ CreateUDPPacket failed");
    free(overhead);
    return;
  }

  for(;;){
    camera_fb_t* fb = nullptr;
    if (xQueueReceive(s_fbq, &fb, pdMS_TO_TICKS(5)) != pdTRUE)continue;
    g_tx_sending = true;        
    frame_idx++;

    const uint8_t* src = fb->buf;
    const size_t   total = fb->len;

    const uint16_t DATA_PER_CHUNK = MAX_CHUNK - MINI_HDR;
    const uint16_t chunk_total = (total + (MAX_CHUNK - MINI_HDR) - 1) / (MAX_CHUNK - MINI_HDR);
    //payload = chunk 2 bytes + chunk total 2 bytes + real payload 1396 bytes;

    size_t offset = 0;

    for(uint16_t ci = 0;ci<chunk_total; ci ++){
      size_t remain = total - offset;
      const uint16_t data_len = (remain > DATA_PER_CHUNK) ? DATA_PER_CHUNK : (uint16_t)remain;
      uint32_t crc_packet;
      // header update and reset pay load
      
      pack->header->id = ID_PACKET;
      pack->header->cmd   = CMD_PACKET;
      pack->header->payload_size  = (uint16_t)(MINI_HDR + data_len);
      //reset payload
      pack->payload_tail_index = 0;


      // --- เติม mini-header: chunk_idx (LE), chunk_total (LE) ---
      uint8_t mini[MINI_HDR];

      mini[0] = (uint8_t)camera_config.pixel_format;
      mini[1] = (uint8_t)((ci+1) & 0xFF);
      mini[2] = (uint8_t)((ci+1) >> 8);
      mini[3] = (uint8_t)(chunk_total & 0xFF);
      mini[4] = (uint8_t)(chunk_total >> 8);
      mini[5] = (uint8_t)camera_config.frame_size;
      mini[6] = 0;
      mini[7] = 0;
      

      //add chunk and total chunk
      if (!AppendBufferPayloadUDPPacket(pack, mini, sizeof(mini))) {
        printf("❌ append mini-header failed\n");
        break;
      }

      // --- add picture payload  ---
      if (!AppendBufferPayloadUDPPacket(pack, src + offset, data_len)) {
        printf("❌ append data failed\n");
        break;
      }

      if (!IsPayloadCompletedUDPPacket(pack)) {
        printf("❌ Payload not complete\n");
        FreeUDPPacket(pack);
        pack = nullptr;
        return;
      }



      uint8_t* hb = ToBytesUDPPacketHeader(pack->header);
      if (!hb) {
        printf("❌ ToBytesUDPPacketHeader failed\n");
        break; // หรือ return
      }
      memcpy(txbuf, hb, UDPPACKETHEADER_SIZE);
      memcpy(txbuf + UDPPACKETHEADER_SIZE,   pack->payload, pack->header->payload_size);
      if(CRC) {
      crc_packet = crc32_calc(txbuf, (size_t)UDPPACKETHEADER_SIZE + pack->header->payload_size);
      memcpy(txbuf + UDPPACKETHEADER_SIZE + pack->header->payload_size, &crc_packet, 4);
      }

      
      int need = (int)(UDPPACKETHEADER_SIZE + pack->header->payload_size + (CRC ? 4 : 0));
      //  sent = c_udp_send(txbuf, sizeof(txbuf));
      int sent = c_udp_send(txbuf, need);
        if (sent != need) {                      // UDP ปกติไม่ partial; สำเร็จควร == need
            for (int i = 0; i < 3 && sent != need; ++i) {
                vTaskDelay(pdMS_TO_TICKS(1));
                sent = c_udp_send(txbuf, need);
            }
        }
      free(hb);  
      offset += data_len;

      if (!printed_first_frame /* && ci < 3 */&& print_debug_packet) {


        
        const size_t HDR  = UDPPACKETHEADER_SIZE;
        const size_t plen = pack->header->payload_size;
        const uint8_t* pay = txbuf + HDR;

        printf("[HEAD] %02X %02X %02X %02X\n",
              (unsigned)txbuf[0], (unsigned)txbuf[1],
              (unsigned)txbuf[2], (unsigned)txbuf[3]);

        printf("[ID  ] %02X %02X\n",
              (unsigned)txbuf[4], (unsigned)txbuf[5]);

        printf("[CMD ] %02X\n", (unsigned)txbuf[6]);

        printf("[PLEN] %02X %02X  (%u)\n",
              (unsigned)txbuf[7], (unsigned)txbuf[8], (unsigned)plen);

        printf("[chunk/total] %02X %02X/%02X %02X\n",
              (unsigned)txbuf[UDPPACKETHEADER_SIZE+MODE], (unsigned)txbuf[UDPPACKETHEADER_SIZE+MODE+1], 
              (unsigned)txbuf[UDPPACKETHEADER_SIZE+MODE+CHUNK], (unsigned)txbuf[UDPPACKETHEADER_SIZE+MODE+CHUNK+1]);


        printf("[data]");
        for (size_t i = 0; i < 32; ++i) {
          printf(" %02X", (unsigned)txbuf[i]);
        }
        printf("\n");

        size_t wire_len = HDR +  pack->header->payload_size + (CRC ? 4 : 0);
        size_t take = (wire_len < 32) ? wire_len : 32;
        size_t start = wire_len - take;

        printf("[data (last %u)] =", (unsigned)take);
        for (size_t i = start; i < wire_len; ++i) printf(" %02X", (unsigned)txbuf[i]);
        printf("\n");


        // ---- CRC (ถ้าเปิด) อยู่ถัดจาก payload ทันที ----
        #if CRC
        if (plen + HDR + 4 <= sizeof(txbuf)) {
          printf("[CRC ] %02X %02X %02X %02X\n",
                (unsigned)txbuf[HDR + plen + 0],
                (unsigned)txbuf[HDR + plen + 1],
                (unsigned)txbuf[HDR + plen + 2],
                (unsigned)txbuf[HDR + plen + 3]);
        }
        #else
        printf("[CRC ] <OFF>\n");
        #endif

        printf("txbuf capacity = %u bytes\n", (unsigned)sizeof(txbuf));
        
        if (sent >= 0) {
        printf("sent done\n");
        } else {
          printf("send failed\n");
        }

        printf("===================\n");



      }


    }
    if (!printed_first_frame) {
      printed_first_frame = true;
    }
    
    esp_camera_fb_return(fb);
    g_tx_sending = false; 
    
  }
  FreeUDPPacket(pack);
}


void rx_task(void*){
  if (!c_udp_listen(RX_PORT)) {
   printf("[RX] listen failed\n");
    for(;;) delay(1000);
  }
  c_udp_set_recv_timeout_ms(100); 

  static uint8_t rx[64];
  uint32_t sip; uint16_t sport;
  for(;;){
    
    int n = c_udp_recv(rx, sizeof(rx), &sip, &sport);
  
    
      if(on_rx_packet(rx, n, sip, sport)) {
        printf("packet ok\n");
        if(extract_pkt(&command,rx,n)){
          printf("extract ok\n");
          handle_command(&command);
        // printf("sig=%.4s id=%u cmd=0x%02X payload=%u\n",
        //        command.hdr.signature,
        //        (unsigned)command.hdr.id,
        //        command.hdr.cmd,
        //        (unsigned)command.hdr.payload_size);

        // // พิมพ์ payload (hex)
        // for (int i = 0; i < command.payload_len; ++i) {
        //   printf("%02X ", command.payload[i]);
        //   if ((i + 1) % 16 == 0) printf("\n");
        // }
        // if (command.payload_len % 16) printf("\n");


        }
      }
  }


}


