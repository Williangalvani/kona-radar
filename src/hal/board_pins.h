// Waveshare ESP32-S3-Touch-AMOLED-1.43 pin map (from the Waveshare wiki
// interface table and the 1.43 demo sources).
#pragma once

// 1.43" round AMOLED, 466 x 466, QSPI (SH8601 or CO5300 controller)
#define PIN_LCD_CS 9
#define PIN_LCD_SCLK 10
#define PIN_LCD_D0 11
#define PIN_LCD_D1 12
#define PIN_LCD_D2 13
#define PIN_LCD_D3 14
#define PIN_LCD_RST 21
#define PIN_LCD_EN 42  // AMOLED_EN in the wiki table

#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Shared I2C bus: FT3168 touch (0x38), QMI8658 IMU, PCF85063 RTC. Also on
// the 4-pin I2C header.
#define PIN_I2C_SDA 47
#define PIN_I2C_SCL 48
#define TOUCH_I2C_ADDR 0x38

#define PIN_BOOT_BUTTON 0
#define PIN_BAT_ADC 4
