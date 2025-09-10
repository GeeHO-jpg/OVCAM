
// Optionally force a driver version:
// #define OVCAMSCCB_USE_IDF5
// #define OVCAMSCCB_USE_IDF4

#include <Arduino.h>

#define I2Ctest
#define OV2640
#include <ovcam.h>

static const int SDA_GPIO = SIOD_GPIO_NUM;
static const int SCL_GPIO = SIOC_GPIO_NUM;
static const uint32_t I2C_FREQ = 100000;
static const uint8_t  OV2640_ADDR7 = 0x30;

// ย้ายแมโครเหล่านี้มา define ในสเก็ตช์ชั่วคราว จนกว่าจะมี abstraction
// #define BANK_SEL     0xFF
// #define BANK_DSP     0x00
// #define BANK_SENSOR  0x01
// #define REG_PIDH     0x0A
// #define REG_PIDL     0x0B

inline bool sel_sensor(uint8_t a){ return sccb_write8(a, BANK_SEL, BANK_SENSOR) == SCCB_OK; }
inline bool sel_dsp(uint8_t a){    return sccb_write8(a, BANK_SEL, BANK_DSP)    == SCCB_OK; }

void scan_bus(){
  Serial.println("I2C scan (7-bit):");
  for (uint8_t a=1; a<127; ++a){
    if (sccb_probe(a) == SCCB_OK){
      Serial.printf("  found: 0x%02X\n", a); delay(3);
    }
  }
  Serial.println("scan done.");
}

void setup(){
  Serial.begin(115200); delay(300);
  sccb_log_set_level(SCCB_LOG_DEBUG);

  if (sccb_init(SDA_GPIO, SCL_GPIO, I2C_FREQ) != SCCB_OK){
    SCCB_LOGE("demo", "SCCB init failed"); return;
  }
  SCCB_LOGI("demo", "SCCB init OK (SDA=%d SCL=%d %luHz)", SDA_GPIO, SCL_GPIO, (unsigned long)I2C_FREQ);

  scan_bus();

  if (sccb_probe(OV2640_ADDR7) != SCCB_OK){
    SCCB_LOGW("demo", "No ACK at 0x30 (check XCLK/RESET/PWDN/wiring)"); return;
  }

  uint8_t h=0,l=0;
  if (!sel_sensor(OV2640_ADDR7) ||
      sccb_read8(OV2640_ADDR7, REG_PIDH, &h) != SCCB_OK ||
      sccb_read8(OV2640_ADDR7, REG_PIDL, &l) != SCCB_OK){
    SCCB_LOGE("demo", "PID read failed"); return;
  }
  uint16_t pid = ((uint16_t)h<<8) | l;
  SCCB_LOGI("demo", "PID=0x%04X (H=0x%02X L=0x%02X)", pid, h, l);
  SCCB_LOGD("I2C", "I2C connected");
}

void loop(){}
