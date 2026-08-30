#pragma once

// Waveshare ESP32-S3-Touch-AMOLED-1.75
// Source: github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75

#define XPOWERS_CHIP_AXP2101

#define BOARD_NAME "1.75 AMOLED"
#define PET_SCALE 5

#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40
#define TP_ADDR 0x5A

#define I2S_MCK_IO 42
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define PA 46

#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3
#define SDMMC_CS 41
