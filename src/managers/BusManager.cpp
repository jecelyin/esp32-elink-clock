#include "BusManager.h"

void BusManager::begin() {
    // Start with neither active, or explicitly default to one.
    // Let's default to safe state (NONE) and let drivers request what they need.
    currentMode = MODE_NONE;
}

void BusManager::requestDisplay() {
    if (currentMode == MODE_DISPLAY) return;
    digitalWrite(CODEC_EN, LOW);

    if (currentMode == MODE_I2C) {
        Wire.end();
    }

    // Initialize SPI for Display
    // Pins: SCK=18, MOSI=23, CS=19, DC=16, RST=4, BUSY=13
    // EPD_CS, EPD_DC, EPD_RST, EPD_BUSY are handled by GxEPD2 or manual digitalWrite
    // but the SPI Bus itself needs SCK and MOSI.
    
    // Note: SPI.begin(sck, miso, mosi, ss)
    // We don't really use MISO for this display, pass -1.
    SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS); 
    
    currentMode = MODE_DISPLAY;
    Serial.println("BusManager: Switched to Display (SPI)");
}

void BusManager::requestI2C() {
    if (currentMode == MODE_I2C) return;
    digitalWrite(CODEC_EN, HIGH);
    if (currentMode == MODE_DISPLAY) {
        SPI.end();
    }

    // Initialize I2C
    // Pins: SDA=18, SCL=23
    Wire.begin(I2C_SDA, I2C_SCL);

    currentMode = MODE_I2C;
    Serial.println("BusManager: Switched to I2C");
}
