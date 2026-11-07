#include "amoled_panel.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_rom_sys.h>

#include "board_pins.h"
#include "brightness.h"
#include "config.h"
#include "esp_lcd_sh8601.h"

namespace hal {
namespace {

constexpr spi_host_device_t kLcdHost = SPI2_HOST;
constexpr uint8_t kSh8601Id = 0x86;
constexpr uint32_t kOpWriteCmd = 0x02;

// Init sequences from Waveshare's 1.43 demo (lcd_bsp.c). Brightness starts at
// 0 so the garbage in panel RAM is never visible; the app raises it after
// the first frame.
const uint8_t kZero[] = {0x00};
const uint8_t kTearScanline[] = {0x01, 0xD1};
const uint8_t kBrightCtrl[] = {0x20};
const uint8_t kSpiMode[] = {0x80};
const uint8_t kHbm[] = {0xFF};

const sh8601_lcd_init_cmd_t kSh8601Init[] = {
    {0x11, kZero, 0, 120},         // sleep out
    {0x44, kTearScanline, 2, 0},   // tear scanline
    {0x35, kZero, 1, 0},           // tear on
    {0x53, kBrightCtrl, 1, 10},    // brightness control on
    {0x51, kZero, 1, 10},          // brightness 0
    {0x29, kZero, 0, 10},          // display on
};

const sh8601_lcd_init_cmd_t kCo5300Init[] = {
    {0x11, kZero, 0, 80},          // sleep out
    {0xC4, kSpiMode, 1, 0},        // SPI mode control
    {0x53, kBrightCtrl, 1, 1},     // brightness control on
    {0x63, kHbm, 1, 1},            // HBM brightness
    {0x51, kZero, 1, 1},           // brightness 0
    {0x29, kZero, 0, 10},          // display on
};

void pinOut(int pin, int level) { gpio_set_level(static_cast<gpio_num_t>(pin), level); }

void bbSendByte(uint8_t v) {
  for (int i = 0; i < 8; ++i) {
    pinOut(PIN_LCD_D0, (v & 0x80) ? 1 : 0);
    v <<= 1;
    pinOut(PIN_LCD_SCLK, 0);
    pinOut(PIN_LCD_SCLK, 1);
  }
}

void setD0Mode(gpio_mode_t mode) {
  gpio_config_t c = {};
  c.pin_bit_mask = 1ULL << PIN_LCD_D0;
  c.mode = mode;
  c.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&c);
}

// Reads register 0xDA (ID1) with a single-lane bit-banged read, the same
// sequence Waveshare uses to tell the SH8601 and CO5300 variants apart.
uint8_t readPanelId() {
  gpio_config_t c = {};
  c.pin_bit_mask = (1ULL << PIN_LCD_CS) | (1ULL << PIN_LCD_SCLK) | (1ULL << PIN_LCD_D0) |
                   (1ULL << PIN_LCD_D1) | (1ULL << PIN_LCD_D2) | (1ULL << PIN_LCD_D3) |
                   (1ULL << PIN_LCD_RST);
  c.mode = GPIO_MODE_OUTPUT;
  c.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&c);
  pinOut(PIN_LCD_CS, 0);
  pinOut(PIN_LCD_SCLK, 0);

  pinOut(PIN_LCD_RST, 1);
  delay(120);
  pinOut(PIN_LCD_RST, 0);
  delay(120);
  pinOut(PIN_LCD_RST, 1);
  delay(120);

  bbSendByte(0x03);  // read opcode
  bbSendByte(0x00);
  bbSendByte(0xDA);
  bbSendByte(0x00);

  uint8_t v = 0;
  for (int i = 0; i < 8; ++i) {
    pinOut(PIN_LCD_SCLK, 0);
    setD0Mode(GPIO_MODE_INPUT);
    esp_rom_delay_us(1);
    v = static_cast<uint8_t>((v << 1) | gpio_get_level(static_cast<gpio_num_t>(PIN_LCD_D0)));
    setD0Mode(GPIO_MODE_OUTPUT);
    pinOut(PIN_LCD_SCLK, 1);
    esp_rom_delay_us(1);
  }
  pinOut(PIN_LCD_CS, 1);
  return v;
}

struct DoneCtx {
  AmoledPanel::FlushDoneCb cb;
  void* ctx;
};
DoneCtx g_done;

bool IRAM_ATTR onColorDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) {
  if (g_done.cb) g_done.cb(g_done.ctx);
  return false;
}

}  // namespace

bool AmoledPanel::begin(size_t maxTransferBytes, FlushDoneCb onDone, void* ctx) {
  g_done = {onDone, ctx};

  pinMode(PIN_LCD_EN, OUTPUT);
  digitalWrite(PIN_LCD_EN, HIGH);
  delay(10);

  rawId_ = readPanelId();
#if RADAR_PANEL_FORCE == 1
  type_ = PanelType::SH8601;
#elif RADAR_PANEL_FORCE == 2
  type_ = PanelType::CO5300;
#else
  type_ = rawId_ == kSh8601Id ? PanelType::SH8601 : PanelType::CO5300;
#endif
  log_i("AMOLED ID 0x%02x -> %s", rawId_, name());

  spi_bus_config_t bus = {};
  bus.sclk_io_num = PIN_LCD_SCLK;
  bus.data0_io_num = PIN_LCD_D0;
  bus.data1_io_num = PIN_LCD_D1;
  bus.data2_io_num = PIN_LCD_D2;
  bus.data3_io_num = PIN_LCD_D3;
  bus.data4_io_num = -1;
  bus.data5_io_num = -1;
  bus.data6_io_num = -1;
  bus.data7_io_num = -1;
  bus.max_transfer_sz = static_cast<int>(maxTransferBytes);
  if (spi_bus_initialize(kLcdHost, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
    log_e("spi_bus_initialize failed");
    return false;
  }

  esp_lcd_panel_io_spi_config_t ioCfg = {};
  ioCfg.cs_gpio_num = PIN_LCD_CS;
  ioCfg.dc_gpio_num = -1;
  ioCfg.spi_mode = 0;
  ioCfg.pclk_hz = RADAR_PANEL_PCLK_HZ;
  ioCfg.trans_queue_depth = 10;
  ioCfg.on_color_trans_done = onColorDone;
  ioCfg.user_ctx = nullptr;
  ioCfg.lcd_cmd_bits = 32;
  ioCfg.lcd_param_bits = 8;
  ioCfg.flags.quad_mode = 1;
  esp_lcd_panel_io_handle_t io = nullptr;
  if (esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(kLcdHost), &ioCfg, &io) !=
      ESP_OK) {
    log_e("esp_lcd_new_panel_io_spi failed");
    return false;
  }

  sh8601_vendor_config_t vendor = {};
  vendor.flags.use_qspi_interface = 1;
  if (type_ == PanelType::SH8601) {
    vendor.init_cmds = kSh8601Init;
    vendor.init_cmds_size = sizeof(kSh8601Init) / sizeof(kSh8601Init[0]);
  } else {
    vendor.init_cmds = kCo5300Init;
    vendor.init_cmds_size = sizeof(kCo5300Init) / sizeof(kCo5300Init[0]);
  }

  esp_lcd_panel_dev_config_t devCfg = {};
  devCfg.reset_gpio_num = PIN_LCD_RST;
  devCfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  devCfg.bits_per_pixel = 16;
  devCfg.vendor_config = &vendor;
  esp_lcd_panel_handle_t panel = nullptr;
  if (esp_lcd_new_panel_sh8601(io, &devCfg, &panel) != ESP_OK) {
    log_e("esp_lcd_new_panel_sh8601 failed");
    return false;
  }
  esp_lcd_panel_reset(panel);
  esp_lcd_panel_init(panel);
  esp_lcd_panel_set_gap(panel, type_ == PanelType::CO5300 ? 6 : 0, 0);
  esp_lcd_panel_disp_on_off(panel, true);

  io_ = io;
  panel_ = panel;
  return true;
}

bool AmoledPanel::reinit() {
  if (!panel_) return false;
  auto* panel = static_cast<esp_lcd_panel_handle_t>(panel_);
  if (esp_lcd_panel_reset(panel) != ESP_OK || esp_lcd_panel_init(panel) != ESP_OK) return false;
  esp_lcd_panel_set_gap(panel, type_ == PanelType::CO5300 ? 6 : 0, 0);
  esp_lcd_panel_disp_on_off(panel, true);
  setBrightness(brightness_);
  return true;
}

bool AmoledPanel::drawBitmap(int x1, int y1, int x2, int y2, const void* pixels) {
  if (!panel_) return false;
  return esp_lcd_panel_draw_bitmap(static_cast<esp_lcd_panel_handle_t>(panel_), x1, y1, x2 + 1, y2 + 1, pixels) ==
         ESP_OK;
}

void AmoledPanel::setBrightness(uint8_t percent) {
  if (!io_) return;
  if (percent > 100) percent = 100;
  brightness_ = percent;
  const uint8_t level = radar::panelLevel(percent);
  esp_lcd_panel_io_tx_param(static_cast<esp_lcd_panel_io_handle_t>(io_),
                            static_cast<int>((kOpWriteCmd << 24) | (0x51 << 8)), &level, 1);
}

void AmoledPanel::setDisplayOn(bool on) {
  if (panel_) esp_lcd_panel_disp_on_off(static_cast<esp_lcd_panel_handle_t>(panel_), on);
}

}  // namespace hal
