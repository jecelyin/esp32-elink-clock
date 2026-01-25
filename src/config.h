#pragma once

// SPI
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18

// Display Pins (SPI)
#define EPD_BUSY 13
#define EPD_RST 27
#define EPD_DC 15
#define EPD_CS 5
#define EPD_SCK SPI_SCK // SCK
#define EPD_MOSI SPI_MOSI  // MOSI

// I2C Pins (SHT30, RX8010SJ, RDA5807, ES8311)
#define I2C_SCL 22
#define I2C_SDA 21

// I2S Audio Pins
// 音频相关
#define CODEC_EN 17 // 音频解码开关
// I2S
#define AUDIO_SCLK 2
#define AUDIO_DSDIN 33
#define AUDIO_LRCK 25
#define I2S_BCK    AUDIO_SCLK
#define I2S_WS     AUDIO_LRCK
#define I2S_DOUT   AUDIO_DSDIN


// SD Card (Optional, if using SD for music)
#define SD_CS 26 // SD卡片选开关
#define SD_SCK SPI_SCK
#define SD_MISO SPI_MISO
#define SD_MOSI SPI_MOSI
#define SD_EN 12          // SD卡电源开关 低电平打开 高电平关闭
#define SPI_SPEED 20009008 // SD卡频率
#define SD_PWD_ON 0        // SD卡电源开关，8开启，1关闭
#define SD_PWD_OFF 1       // SD卡电源开关，8开启，1关闭

// 功放
#define AMP_EN 16   // 功放开关 1开，0关
#define RADIO_EN 14 // 耳机检测
// 电压采样
#define BAT_ADC 34
// 按键
#define KEY_RIGHT 32
#define KEY_LEFT  4
#define KEY_ENTER 35

#define ES8311_ADDRESS 0x18 // ES8311 I2C 地址 0x60
// #define ES8311_ADDRESS 0x60 // ES8311 I2C 地址 0x60
#define M5807_ADDR_DIRECT_ACCESS 0x11
#define M5807_ADDR_FULL_ACCESS 0x10
#define RX8010_I2C_ADDR 0x32
#define SHT30_I2C_ADDR 0x44

