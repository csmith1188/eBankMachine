#include "eBankMachine.h"

void LCDWrapper::begin(LCDMode mode) {
    currentMode = mode;

#ifdef USE_LCD_I2C
    if (mode == LCD_MODE_I2C) {
        Wire.begin(SDA_PIN, SCL_PIN);
        lcd_i2c.init();
        lcd_i2c.backlight();
    }
#endif

#ifdef USE_LCD_PARALLEL
    if (mode == LCD_MODE_PARALLEL) {
        lcd_parallel.begin(16, 2);
    }
#endif

    if (mode == LCD_MODE_SERIAL) {
        Serial.begin(115200);
    }
}

void LCDWrapper::clear() {
    switch (currentMode) {
#ifdef USE_LCD_I2C
        case LCD_MODE_I2C: lcd_i2c.clear(); break;
#endif
#ifdef USE_LCD_PARALLEL
        case LCD_MODE_PARALLEL: lcd_parallel.clear(); break;
#endif
        case LCD_MODE_SERIAL: Serial.println("[LCD CLEAR]"); break;
    }
}

void LCDWrapper::setCursor(uint8_t col, uint8_t row) {
    switch (currentMode) {
#ifdef USE_LCD_I2C
        case LCD_MODE_I2C: lcd_i2c.setCursor(col, row); break;
#endif
#ifdef USE_LCD_PARALLEL
        case LCD_MODE_PARALLEL: lcd_parallel.setCursor(col, row); break;
#endif
        case LCD_MODE_SERIAL:
            Serial.print("[CURSOR "); Serial.print(col); Serial.print(","); Serial.print(row); Serial.println("]");
            break;
    }
}

void LCDWrapper::print(const String& text) {
    switch (currentMode) {
#ifdef USE_LCD_I2C
        case LCD_MODE_I2C: lcd_i2c.print(text); break;
#endif
#ifdef USE_LCD_PARALLEL
        case LCD_MODE_PARALLEL: lcd_parallel.print(text); break;
#endif
        case LCD_MODE_SERIAL: Serial.print(text); break;
    }
}

void LCDWrapper::print(const char* text) { print(String(text)); }
void LCDWrapper::print(int value) { print(String(value)); }
void LCDWrapper::print(long value) { print(String(value)); }