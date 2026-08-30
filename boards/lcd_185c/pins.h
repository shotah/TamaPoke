#pragma once

// Waveshare ESP32-S3-Touch-LCD-1.85C V2 (BOX + speaker).
// V1 is a different pinout (PCM5101) — do not flash this build on V1.
// Source: github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C

#define BOARD_NAME "1.85C V2"
#define PET_SCALE 4

// Round ST77916 360x360 over QSPI. LCD_RST is TCA9554 EXIO2, not a GPIO.
#define LCD_SDIO0 46
#define LCD_SDIO1 45
#define LCD_SDIO2 42
#define LCD_SDIO3 41
#define LCD_SCLK 40
#define LCD_CS 21
#define LCD_TE 18
#define LCD_BL 5
#define LCD_WIDTH 360
#define LCD_HEIGHT 360

#define IIC_SDA 11
#define IIC_SCL 10
#define TP_INT 4
#define TP_ADDR 0x15

#define TCA9554_ADDR 0x20
#define EXIO_TP_RST 1
#define EXIO_LCD_RST 2
#define EXIO_SD_CS 3

#define I2S_MCK_IO 2
#define I2S_BCK_IO 48
#define I2S_DI_IO 39
#define I2S_WS_IO 38
#define I2S_DO_IO 47
#define PA 15

#define SDMMC_CLK 14
#define SDMMC_CMD 17
#define SDMMC_DATA 16

#define BAT_ADC 8
