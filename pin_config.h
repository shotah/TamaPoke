#pragma once

// Pins for Waveshare ESP32-S3-Touch-LCD-1.85C V2 (BOX + speaker).
// Source: github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85C
//   Arduino/examples/01_lvgl_example/{I2C_Driver,Display_ST77916,Touch_CST816}.h
//   Arduino/examples/03_audio_out_no_tf/03_audio_out_no_tf.ino
//   Arduino/examples/04_SDMMC_Test/04_SDMMC_Test.ino
//
// V2 (Rev2.0 sticker): I2C on GPIO10/11, ES8311, PA on GPIO15.
// V1 is a different pinout (PCM5101) — do not flash this build on V1.

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

// Original TamaPoke layout was 466x466. Scale draw/hit coords with these.
#define DESIGN_W 466
#define DESIGN_H 466
#define SX(x) ((int)((long)(x) * LCD_WIDTH / DESIGN_W))
#define SY(y) ((int)((long)(y) * LCD_HEIGHT / DESIGN_H))

// Shared I2C: TCA9554 @ 0x20, CST816 @ 0x15, PCF85063, ES8311 @ 0x18
#define IIC_SDA 11
#define IIC_SCL 10

#define TP_INT 4
#define TP_ADDR 0x15

#define TCA9554_ADDR 0x20
#define EXIO_TP_RST 1   // EXIO1
#define EXIO_LCD_RST 2  // EXIO2
#define EXIO_SD_CS 3    // EXIO3

// ES8311 (V2). Official example also drives MCLK on GPIO2.
#define I2S_MCK_IO 2
#define I2S_BCK_IO 48
#define I2S_DI_IO 39
#define I2S_WS_IO 38
#define I2S_DO_IO 47
#define PA 15

// microSD 1-bit SDMMC
#define SDMMC_CLK 14
#define SDMMC_CMD 17
#define SDMMC_DATA 16

// Battery sense: resistor divider into ADC (Waveshare example uses GPIO8)
#define BAT_ADC 8
