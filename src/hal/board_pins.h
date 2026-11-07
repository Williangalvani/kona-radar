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
// AMOLED_EN in the wiki table. On the schematic it reaches the panel connector
// only through R9, marked NC (not fitted), and Waveshare's demos never drive
// it; setting it high is harmless.
#define PIN_LCD_EN 42

#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Shared I2C bus: FT3168 touch (0x38), QMI8658 IMU (0x6B), PCF85063 RTC
// (0x51). Also on the 4-pin I2C header. 10 k pull-ups to 3V3 on the board.
// The FT3168 runs I2C at up to 400 kHz.
#define PIN_I2C_SDA 47
#define PIN_I2C_SCL 48
#define I2C_FREQ_HZ 400000
#define TOUCH_I2C_ADDR 0x38
// Not connected to the ESP32 (panel connector J9 pins 7 and 9 are NC).
#define PIN_TOUCH_INT -1
#define PIN_TOUCH_RST -1

#define PIN_BOOT_BUTTON 0
#define PIN_BAT_ADC 4
