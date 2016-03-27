
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
byte temp_char[8] = {B01110, B01010, B01010, B01110, B01110, B11111, B11111, B01110};
byte hum_char[8] = {B00100, B01110, B11111, B00100, B10001, B00100, B10001, B00100};
byte bell_Char[8] = {B00100, B01110, B01110, B01110, B11111, B11111, B00100, B00000};
byte arrow_Char[8] = {B00000, B00000, B10000, B10000, B10111, B10011, B10101, B01000};

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2); // set up the LCD's number of columns and rows
  lcd.createChar(1, temp_char);
  lcd.createChar(2, hum_char);
  lcd.createChar(3, bell_Char);
  lcd.createChar(4, arrow_Char);

  
  lcd.clear();
  lcd.write(1);
  lcd.write(2);
  lcd.write(3);
  lcd.write(4);
}

void loop() {
}
