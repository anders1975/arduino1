#include "Wire.h" // For I2C
#include "LiquidCrystal_I2C.h"
LiquidCrystal_I2C lcd(0x3F,20,4); // 0x27 is often the default I2C bus address

const long startScreenInterval = 2000;
const long maxPulseInterval = 10000;
volatile long counter2,timePrevious2,timeRecent2 = 0;
volatile long counter3,timePrevious3,timeRecent3 = 0;
volatile long counter18,timePrevious18,timeRecent18 = 0;
volatile long counter19,timePrevious19,timeRecent19 = 0;

void setup()
{
  lcd.init();
  lcd.backlight();
  pinMode(3, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(3), ISR_3, FALLING);
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), ISR_2, FALLING);
  pinMode(18, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(18), ISR_18, FALLING);
  pinMode(19, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(19), ISR_19, FALLING);
  startScreen();
}

void ISR_2()
{
  timePrevious2 = timeRecent2;
  timeRecent2 = millis();
  counter2++;
}

void ISR_3()
{
  timePrevious3 = timeRecent3;
  timeRecent3 = millis();
  counter3++;
}

void ISR_18()
{
  timePrevious18 = timeRecent18;
  timeRecent18 = millis();
  counter18++;
}

void ISR_19()
{
  timePrevious19 = timeRecent19;
  timeRecent19 = millis();
  counter19++;
}

void loop()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(millis() - timeRecent2 > maxPulseInterval ? 0 : 60000/(timeRecent2-timePrevious2));
  lcd.setCursor(6,0);
  lcd.print(counter2);

  lcd.setCursor(0, 1);
  lcd.print(millis() - timeRecent3 > maxPulseInterval ? 0 : 60000/(timeRecent3-timePrevious3));
  lcd.setCursor(6,1);
  lcd.print(counter3);

  lcd.setCursor(0, 2);
  lcd.print(millis() - timeRecent18 > maxPulseInterval ? 0 : 60000/(timeRecent18-timePrevious18));
  lcd.setCursor(6,2);
  lcd.print(counter18);

  lcd.setCursor(0, 3);
  lcd.print(millis() - timeRecent19 > maxPulseInterval ? 0 : 60000/(timeRecent19-timePrevious19));
  lcd.setCursor(6,3);
  lcd.print(counter19);


  lcd.setCursor(13,3);
  lcd.print(millis()/1000);
  delay(1000);
}

void startScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("X");
  lcd.setCursor(0, 1);
  lcd.print("X   Impuls/minut");
  lcd.setCursor(0, 2);
  lcd.print("X");
  lcd.setCursor(0, 3);
  lcd.print("X");
  delay(startScreenInterval);

  lcd.clear();
  lcd.setCursor(6, 0);
  lcd.print("X");
  lcd.setCursor(6, 1);
  lcd.print("X Impuls tot");
  lcd.setCursor(6, 2);
  lcd.print("X");
  lcd.setCursor(6, 3);
  lcd.print("X");
  delay(startScreenInterval);

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Sekunder sedan start");
  lcd.setCursor(13, 3);
  lcd.print("X");

  delay(startScreenInterval);

}