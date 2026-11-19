// ============================
// FILE: LCDWrapper.cpp
// ============================
#include "eBankMachine.h"

void LCDWrapper::begin() {
#ifdef USE_LCD_I2C
    Wire.begin(21, 22); // SDA, SCL
    lcd.init();
    lcd.backlight();
    usingLCD = true;
#else
    Serial.begin(115200);
    Serial.println("I2C not enabled — using SERIAL mode");
    usingLCD = false;
#endif
}

void LCDWrapper::clear() {
    if (usingLCD) lcd.clear();
    else Serial.println("\n[LCD CLEAR]");
}

void LCDWrapper::setCursor(uint8_t col, uint8_t row) {
    if (usingLCD) lcd.setCursor(col, row);
    else {
        Serial.print("\n[CURSOR ");
        Serial.print(col);
        Serial.print(",");
        Serial.print(row);
        Serial.print("] ");
    }
}

void LCDWrapper::print(char c) {
    if (usingLCD) lcd.print(c);
    else Serial.print(c);
}

void LCDWrapper::print(const char* text) {
    if (usingLCD) lcd.print(text);
    else Serial.print(text);
}

void LCDWrapper::print(const String& text) {
    if (usingLCD) lcd.print(text);
    else Serial.print(text);
}

void LCDWrapper::print(const __FlashStringHelper* text) {
    if (usingLCD) lcd.print(text);
    else {
        PGM_P p = reinterpret_cast<PGM_P>(text);
        char c;
        while ((c = pgm_read_byte(p++))) Serial.print(c);
    }
}

void LCDWrapper::print(int value) {
    if (usingLCD) lcd.print(value);
    else Serial.print(value);
}

void LCDWrapper::print(long value) {
    if (usingLCD) lcd.print(value);
    else Serial.print(value);
}