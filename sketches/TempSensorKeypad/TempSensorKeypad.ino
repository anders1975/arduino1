//The full code is in library example file Quick_tour.ino
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <MENWIZ.h>
#include <Keypad.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <dht_decade.h>
#include <DS1307RTC.h>
#include <SD.h>
#include <avr/pgmspace.h>

static float saturationVaporContent(float temperature) __attribute__((__optimize__("O2")));
static int humidityRelativeToAbsolute(int temperature, int humidityRelative) __attribute__((__optimize__("O2")));

// Setup DHT21 sensor framework
dht_decade dht_sensor;
const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D' }
};
byte rowPins[ROWS] = {9,8,7,6}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {5,4,3,2}; //connect to the column pinouts of the keypad

//initialize an instance of class NewKeypad
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 

typedef struct _measure
{
  time_t sampleTime;
  int temperature;
  unsigned int humidityRelative;
  unsigned int humidityAbsolute;
}
_measure;

typedef struct
{
  _measure latestMeasure;
  _measure previousMeasure;
  _measure heatingStartedMeasure;
  _measure coolingStartedMeasure;
  _measure hoursMeasure;
  bool isInflowSensor;
  time_t firstSampleTime;
  time_t latestSampleTime;
  int sampleReturnCode;
  float temperatureCalibration;
  float humidityRelativeCalibration;
  int digitalPin;
  unsigned int numberOfWrites;
  unsigned int numberOfReads;
  char description[5];
}
_sensorData;

const int MAX_SENSOR=6;

_sensorData sensorList[MAX_SENSOR];

typedef struct
{
  AlarmID_t alarmID;
  void (*timerFunction)();
  time_t timerInterval;
}
_timerData;

const int MAX_TIMER=3;

_timerData timerList[MAX_TIMER];
int measuringInterval = 10;
int sdLogInterval = 60;
int humidityRelativeMax = 99;
int humidityRelativeMin = 1;
int humidityAbsoluteMax = 99;
int humidityAbsoluteMin = 1;
int temperatureMax = 80;
int temperatureMin = 1;
boolean showOnlyRespondingSensors = true;
int displayCyclingInterval = 3;
boolean logMarker = false;
String dryingMode = "Started";
int dryingModeHeatingTemperature = 28;
int dryingModeCoolingTemperature = 27;

void readAllSensorsTrigger()
{
  Serial.print(__FUNCTION__);
  Serial.println(now());
  bool heatingStarted = false;
  bool coolingStarted = false;
  int humidityAbsolute = 0;
  for(int count = 0 ; count < MAX_SENSOR ; ++count)
  {
    // Move to previous
    sensorList[count].previousMeasure.sampleTime=sensorList[count].latestMeasure.sampleTime;
    sensorList[count].previousMeasure.humidityRelative=sensorList[count].latestMeasure.humidityRelative;
    sensorList[count].previousMeasure.humidityAbsolute=sensorList[count].latestMeasure.humidityAbsolute;
    sensorList[count].previousMeasure.temperature=sensorList[count].latestMeasure.temperature;
    // Read current
    sensorList[count].latestMeasure.sampleTime = now();
    int returnCode = dht_sensor.read21(sensorList[count].digitalPin);
    sensorList[count].sampleReturnCode = returnCode;
    switch (returnCode)
    {
    case DHTLIB_OK:
      humidityAbsolute = humidityRelativeToAbsolute(dht_sensor.temperature, dht_sensor.humidity + (unsigned int)(sensorList[count].humidityRelativeCalibration*10));
      sensorList[count].latestMeasure.temperature = dht_sensor.temperature + (int)(sensorList[count].temperatureCalibration*10);
      sensorList[count].latestMeasure.humidityAbsolute = humidityAbsolute;
      sensorList[count].latestMeasure.humidityRelative = dht_sensor.humidity + (unsigned int)(sensorList[count].humidityRelativeCalibration*10);
      ++sensorList[count].numberOfWrites;
      if (sensorList[count].numberOfWrites >= 1000)
      {
        sensorList[count].numberOfWrites = 0;
      }
      if (0 == sensorList[count].firstSampleTime)
      {
        sensorList[count].firstSampleTime = now();
      }
      if (sensorList[count].isInflowSensor)
      {
        Serial.print("InflowSensor");
        Serial.print(" ");
        Serial.print(count);
        Serial.print(" ");
        Serial.println(dryingMode);
        if ( dryingMode != "Heating" && sensorList[count].latestMeasure.temperature >= dryingModeHeatingTemperature*10)  
        {
          dryingMode = "Heating";
          heatingStarted = true;
          Serial.println("Heating");
        }
        if ( dryingMode == "Heating" && sensorList[count].latestMeasure.temperature <= dryingModeCoolingTemperature*10)  
        {
          dryingMode = "Cooling";
          coolingStarted = true;
          Serial.println("Cooling");
        }
      }
    break;
    default:
      Serial.print("Sensor error: ,\t");
      Serial.print(returnCode);
      Serial.print(" "); 
      Serial.println(count);
      break;
    }
  }
  if (heatingStarted)
  {
    for(int count = 0 ; count < MAX_SENSOR ; ++count)
    {
      sensorList[count].heatingStartedMeasure = sensorList[count].latestMeasure;
    }
  }
  if (coolingStarted)
  {
    for(int count = 0 ; count < MAX_SENSOR ; ++count)
    {
      sensorList[count].coolingStartedMeasure = sensorList[count].latestMeasure;
    }
  }
}

String twoDigits(int number)
{
  String digits;
  if (number < 10)
  {
    digits = "0";
  }
  digits += number;
  return digits;
}

String yearMonthDayToString(time_t time)
{
  String yearMonthDay = "";
  yearMonthDay += year(time)-2000;
  yearMonthDay += twoDigits(month(time));
  yearMonthDay += twoDigits(day(time));
  return yearMonthDay;
}

String hourMinuteToString(time_t time)
{
  String stringHourMinute = "";
  stringHourMinute += twoDigits(hour(time));
  stringHourMinute += twoDigits(minute(time));
  return stringHourMinute;
}

String hourMinuteSecondToString(time_t time)
{
  String stringHourMinuteSecond = "";
  stringHourMinuteSecond += twoDigits(hour(time));
  stringHourMinuteSecond += twoDigits(minute(time));
  stringHourMinuteSecond += twoDigits(second(time));
  return stringHourMinuteSecond;
}

void SDSetup()
{
  const int chipSelect = 10;
  Serial.println(__FUNCTION__);
  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(53, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect))
  {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("Card initialized.");
}

void SDLogWrite(String dataString)
{
  SDSetup();
  String fileName = yearMonthDayToString(now()) + ".txt";
  Serial.println(fileName);
  // Open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open(fileName.c_str(), FILE_WRITE);
  // If the file is available, write to it:
  if (dataFile)
  {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }  
  // If the file isn't open, pop up an error:
  else
  {
    Serial.println("Error opening " + fileName);
  } 
  SD.end();
}

void logAllSensors()
{
  Serial.println(__FUNCTION__);
  String logEntry = "";
  if (logMarker == true)
  {
    logEntry += "MARK";
    logMarker = false;
  }
  else
  {
    logEntry += dryingMode;
  }
  logEntry += ";";

  for(int count = 0 ; count < MAX_SENSOR ; ++count)
  {
    logEntry += sensorList[count].description;
    logEntry += ";";
    logEntry += sensorList[count].sampleReturnCode;
    logEntry += ";";
    logEntry += sensorList[count].latestMeasure.sampleTime;
    logEntry += ";";
    logEntry += yearMonthDayToString(sensorList[count].latestMeasure.sampleTime);
    logEntry += "-";
    logEntry += hourMinuteSecondToString(sensorList[count].latestMeasure.sampleTime);
    logEntry += ";";
    logEntry += sensorList[count].latestMeasure.temperature;
    logEntry += ";";
    logEntry += sensorList[count].latestMeasure.humidityRelative;
    logEntry += ";";
    logEntry += sensorList[count].latestMeasure.humidityAbsolute;
    logEntry += ";";
  }
  Serial.println(logEntry);
  SDLogWrite(logEntry);
}

void hourTrigger()
{
  Serial.print(__FUNCTION__);
  Serial.println(now());
  for(int count = 0 ; count < MAX_SENSOR ; ++count)
  {
    sensorList[count].hoursMeasure = sensorList[count].latestMeasure;
  }
}

void timerSetupAll()
{
  timerList[0].timerFunction=readAllSensorsTrigger;
  timerList[0].timerInterval=measuringInterval;
  timerList[1].timerFunction=logAllSensors;
  timerList[1].timerInterval=sdLogInterval;
  timerList[2].timerFunction=hourTrigger;
  timerList[2].timerInterval=60*60;
  for(unsigned int count ; count < MAX_TIMER ; count++)
  {
    timerList[count].alarmID=Alarm.timerRepeat(timerList[count].timerInterval, timerList[count].timerFunction);
  }
}

void timerCancelAll()
{
  for(unsigned int count ; count < MAX_TIMER ; count++)
  {
    Alarm.free(timerList[count].alarmID);
  }
}

void timerRestartAll(int unused)
{
  Serial.println(__FUNCTION__);
  timerCancelAll();
  timerSetupAll();
}

menwiz tree;
// create lcd obj using LiquidCrystal lib
//LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // 2004 Sainsmart
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // 1602 , 2004 Yellow

time_t latestScreenUpdate;
int currentSensorShown = 0;
int sensorsSkipped = 0;

const char description_A1[] PROGMEM = "A1";
const char description_A2[] PROGMEM = "A2";
const char description_A3[] PROGMEM = "A3";
const char description_A4[] PROGMEM = "A4";
const char description_A5[] PROGMEM = "A5";
const char description_A6[] PROGMEM = "A6";
const char description_B1[] PROGMEM = "B1";
const char description_B2[] PROGMEM = "B2";
const char description_B3[] PROGMEM = "B3";
const char description_B4[] PROGMEM = "B4";
const char description_B5[] PROGMEM = "B5";
const char description_B6[] PROGMEM = "B6";
const char description_C1[] PROGMEM = "C1";
const char description_C2[] PROGMEM = "C2";
const char description_C3[] PROGMEM = "C3";
const char description_C4[] PROGMEM = "C4";
const char description_C5[] PROGMEM = "C5";
const char description_C6[] PROGMEM = "C6";

const char* const descriptions[] PROGMEM = {
  description_A1,
  description_A2,
  description_A3,
  description_A4,
  description_A5,
  description_A6,
  description_B1,
  description_B2,
  description_B3,
  description_B4,
  description_B5,
  description_B6,
  description_C1,
  description_C2,
  description_C3,
  description_C4,
  description_C5,
  description_C6,
};

int navMenu(){
  char customKey = customKeypad.getKey();
  if(customKey){
    Serial.println(customKey);
    if (customKey == 'D')
    {
      return MW_BTE;
    }
    if (customKey == 'C')
    {
      return MW_BTC;
    }
    if (customKey == 'B')
    {
      return MW_BTD;
    }
    if (customKey == 'A')
    {
      return MW_BTU;
    }
    if (customKey == '*')
    {
      return MW_BTL;
    }
    if (customKey == '#')
    {
      return MW_BTR;
    }
  }
  return MW_BTNULL;
}


void useAsInflowSensor(int sensorNumber){
  Serial.println(__FUNCTION__);
  for(int count = 0 ; count < MAX_SENSOR ; ++count)
  {
    sensorList[count].isInflowSensor = false;
  }
  sensorList[sensorNumber].isInflowSensor = true;
}

void calibrateAll(int sensorNumber){
  Serial.println(__FUNCTION__);
  for(int count = 0 ; count < MAX_SENSOR ; ++count)
  {
    sensorList[count].temperatureCalibration = ((float)(sensorList[sensorNumber].latestMeasure.temperature - sensorList[count].latestMeasure.temperature))/10;
    sensorList[count].humidityRelativeCalibration = ((float)(sensorList[sensorNumber].latestMeasure.humidityRelative - sensorList[count].latestMeasure.humidityRelative))/10;
  }
}

String measureToString(struct _measure measure, String padding)
{
  String string = hourMinuteToString(measure.sampleTime);
  string += padding;
  string += measure.temperature/10;
  string += " ";
  string += measure.humidityRelative/10;
  string += " ";
  string += measure.humidityAbsolute/10;
  return string;
}

void showSensorDefault(int count)
{
  lcd.clear();
  String string = "";
  // LCD line 1
  strcpy(tree.sbuf,"Sensor "); 
  strcat(tree.sbuf, sensorList[count].description); 
  strcat(tree.sbuf, " Temp RH AH");
  // LCD line 2
  strcat(tree.sbuf,"\nNow  ");
  string = measureToString(sensorList[count].latestMeasure, " ");
  strcat(tree.sbuf, string.c_str());
  // LCD line 3
  strcat(tree.sbuf,"\nHeat ");
  string = measureToString(sensorList[count].heatingStartedMeasure, " ");
  strcat(tree.sbuf, string.c_str());
  // LCD line 4
  strcat(tree.sbuf,"\nCool ");
  string = measureToString(sensorList[count].coolingStartedMeasure, " ");
  strcat(tree.sbuf, string.c_str());

  // Flush
  Serial.println(tree.sbuf);
  tree.drawUsrScreen(tree.sbuf);

}

void showNoSensorsResponding()
{
  lcd.clear();
  strcpy(tree.sbuf,"No sensor responding\n");
  strcat(tree.sbuf,"   Not connected?");
  Serial.println(tree.sbuf);
  tree.drawUsrScreen(tree.sbuf);
}

void defaultScreen(){
  time_t currentTime=now();
  if (currentTime > latestScreenUpdate + displayCyclingInterval)
  {
    latestScreenUpdate = currentTime;
    if(currentSensorShown < MAX_SENSOR)
    {
      if (sensorList[currentSensorShown].sampleReturnCode != DHTLIB_OK && showOnlyRespondingSensors)
      {
        ++sensorsSkipped;
      }
      else
      {
        showSensorDefault(currentSensorShown);
      }

      if (currentSensorShown >= MAX_SENSOR-1)
      {
        currentSensorShown = 0;
        if (sensorsSkipped >= MAX_SENSOR)
        {
          showNoSensorsResponding();
        }
        sensorsSkipped = 0;
      }
      else
      {
        ++currentSensorShown;
      }
      Serial.println(currentSensorShown);
    }
    else
    {
      currentSensorShown = 0;
      sensorsSkipped = 0;
    }
  }
}

void statusScreen(int unused)
{
  static  char buf[7];
  strcpy(tree.sbuf,"User screen"); //1st lcd line
  strcat(tree.sbuf,"\nUptime (s): ");
  strcat(tree.sbuf,utoa((unsigned int)(millis()/1000),buf,10));//2nd lcd line
  strcat(tree.sbuf,"\nFree mem  : ");
  strcat(tree.sbuf,utoa((unsigned int)tree.freeRam(),buf,10));//3rd lcd line
  strcat(tree.sbuf,"\n"); //4th lcd line (empty)
  tree.drawUsrScreen(tree.sbuf);
  Alarm.delay(tree.tm_usrScreen * 1000);
}

void showYearMonthDay()
{
  String yearMonthDay = "";
  yearMonthDay += "YYMMDD: ";
  yearMonthDay += yearMonthDayToString(now());
  yearMonthDay.getBytes((unsigned char*)tree.sbuf, yearMonthDay.length()+1);
  Serial.println(tree.sbuf);
  tree.drawUsrScreen(tree.sbuf);
}

int * readDigits(){
  Serial.println(__FUNCTION__);
  const unsigned int DIGITS=6;
  int count=0;
  int number = -1;
  static int digit[DIGITS];
  time_t startTime = now();
  lcd.print("Set value: ");
  Serial.print("Set value: ");
  while(count<DIGITS)
  {
    char key = customKeypad.getKey();
    if (key != NO_KEY)
    {
      lcd.print(key);
      Serial.print(key);
      number = key - '0';
      digit[count]=number;
      Serial.print(number);
      ++count;
      startTime = now(); // Prolong timeout for next digit
    }
    if (now() > startTime + 10)
    {
      digit[0]=-1; // Timeout before all digits were set
      lcd.print("FAIL");
      break;
    }
  }
  Alarm.delay(tree.tm_usrScreen * 1000);
  return digit;
}

void setYearMonthDay(int unused)
{
  lcd.clear();
  showYearMonthDay();
  int* yearMonthDay;
  yearMonthDay=readDigits();
  if (yearMonthDay[0] != -1)
  {
    int YY=10*yearMonthDay[0] + yearMonthDay[1];
    int MM=10*yearMonthDay[2] + yearMonthDay[3];
    int DD=10*yearMonthDay[4] + yearMonthDay[5];
    int hh=hour();
    int mm=minute();
    int ss=second();
    timerCancelAll();
    setTime(hh,mm,ss,DD,MM,YY);
    timerSetupAll();
  }
  Serial.println(now());
  lcd.print(now());
}

void showHourMinuteSecond()
{
  String stringHourMinuteSecond = "";
  stringHourMinuteSecond += "HHMMSS: ";
  stringHourMinuteSecond += hourMinuteSecondToString(now());
  stringHourMinuteSecond.getBytes((unsigned char*)tree.sbuf, stringHourMinuteSecond.length()+1);
  Serial.println(tree.sbuf);
  tree.drawUsrScreen(tree.sbuf);
}

void setHourMinuteSecond(int unused)
{
  lcd.clear();
  showHourMinuteSecond();
  int* hourMinuteSecond;
  hourMinuteSecond=readDigits();
  if (hourMinuteSecond[0] != -1)
  {
    int hh=10*hourMinuteSecond[0] + hourMinuteSecond[1];
    int mm=10*hourMinuteSecond[2] + hourMinuteSecond[3];
    int ss=10*hourMinuteSecond[4] + hourMinuteSecond[5];
    int YY=year();
    int MM=month();
    int DD=day();
    timerCancelAll();
    setTime(hh,mm,ss,DD,MM,YY);
    timerSetupAll();
  }

  Serial.println(now());
  lcd.print(now());
}


void showTime(int unused)
{
  String time="Time: ";
  time += yearMonthDayToString(now());
  time += "-";
  time += hourMinuteSecondToString(now());
  Serial.println(time);
  lcd.clear();
  lcd.print(time);
  Alarm.delay(tree.tm_usrScreen * 1000);
}

static float saturationVaporContent(float temperature)
{
  static const float k[] = {4.7815706, 0.34597292, 0.0099365776, 0.00015612096, 1.9830825E-6, 1.5773396E-8};
  float result = 0.;
  for (int i=0; i<6; ++i)
  {
    result += k[i]*pow(temperature, i);
  }
  return result;
}

static int humidityRelativeToAbsolute(int temperature, int humidityRelative)
{
  return saturationVaporContent(((float)temperature)/10) * humidityRelative/100;
}

void showSensorRecent(int count)
{
  static char buf[12];
  lcd.print("Sensor data: ");
  ++sensorList[count].numberOfReads;
  if (sensorList[count].numberOfReads >= 1000)
  {
    sensorList[count].numberOfReads = 0;
  }
  // LCD line 1
  strcpy(tree.sbuf,"Sensor "); 
  strcat(tree.sbuf, sensorList[count].description); 
  strcat(tree.sbuf, " Temp RH AH");//1st lcd line
  // LCD line 2
  strcat(tree.sbuf,"\nNow  ");
  strcat(tree.sbuf,itoa((int)sensorList[count].latestMeasure.temperature/10,buf,10));
  strcat(tree.sbuf,".");
  strcat(tree.sbuf,itoa(abs((int)sensorList[count].latestMeasure.temperature) % 10,buf,10));
  strcat(tree.sbuf," ");
  strcat(tree.sbuf,itoa((unsigned int)sensorList[count].latestMeasure.humidityRelative/10,buf,10));
  strcat(tree.sbuf,".");
  strcat(tree.sbuf,itoa(abs((int)sensorList[count].latestMeasure.humidityRelative) % 10,buf,10));
  strcat(tree.sbuf," ");
  strcat(tree.sbuf,itoa((unsigned int)sensorList[count].latestMeasure.humidityAbsolute/10,buf,10));
  strcat(tree.sbuf,".");
  strcat(tree.sbuf,itoa(abs((int)sensorList[count].latestMeasure.humidityAbsolute) % 10,buf,10));
  // LCD line 3
  strcat(tree.sbuf,"\nPrev ");
  strcat(tree.sbuf,itoa((int)sensorList[count].previousMeasure.temperature/10,buf,10));
  strcat(tree.sbuf,".");
  strcat(tree.sbuf,itoa(abs((int)sensorList[count].previousMeasure.temperature) % 10,buf,10));
  strcat(tree.sbuf," ");
  strcat(tree.sbuf,itoa((unsigned int)sensorList[count].previousMeasure.humidityRelative/10,buf,10));
  strcat(tree.sbuf,".");
  strcat(tree.sbuf,itoa(abs((int)sensorList[count].previousMeasure.humidityRelative) % 10,buf,10));
  strcat(tree.sbuf," ");
  strcat(tree.sbuf,itoa((unsigned int)sensorList[count].previousMeasure.humidityAbsolute/10,buf,10));
  strcat(tree.sbuf,".");
  strcat(tree.sbuf,itoa(abs((int)sensorList[count].previousMeasure.humidityAbsolute) % 10,buf,10));
  // LCD line 4
  if (sensorList[count].sampleReturnCode != DHTLIB_OK)
  {
    strcat(tree.sbuf,"\n-= NO CONNECTION! =-");
  }
  else if (sensorList[count].isInflowSensor)
  {
    strcat(tree.sbuf, "\nDrying mode: ");
    strcat(tree.sbuf, dryingMode.c_str());
  }
  else
  {
    if (sensorList[count].latestMeasure.humidityRelative < humidityRelativeMax*10 &&
      sensorList[count].latestMeasure.humidityRelative > humidityRelativeMin*10 &&
      sensorList[count].latestMeasure.humidityAbsolute < humidityAbsoluteMax*10 &&
      sensorList[count].latestMeasure.humidityAbsolute > humidityAbsoluteMin*10 &&
      sensorList[count].latestMeasure.temperature < temperatureMax*10 &&
      sensorList[count].latestMeasure.temperature > temperatureMin*10)
    {
      strcat(tree.sbuf,"\n");
      strcat(tree.sbuf,"P: ");
      strcat(tree.sbuf,itoa(sensorList[count].digitalPin,buf,10));
      strcat(tree.sbuf," ");
      strcat(tree.sbuf,"W: ");
      strcat(tree.sbuf,itoa((unsigned int)sensorList[count].numberOfWrites,buf,10));
      strcat(tree.sbuf," ");
      strcat(tree.sbuf,"R: ");
      strcat(tree.sbuf,itoa((unsigned int)sensorList[count].numberOfReads,buf,10));
    }
    else // ALARM
    {
      strcat(tree.sbuf,"\n -= ALARM ! =-");
    }
  }
  Serial.println(tree.sbuf);
  tree.drawUsrScreen(tree.sbuf);

}

void showSensorHistory(int count)
{
  lcd.clear();
  String string = "";
  // LCD line 1
  strcpy(tree.sbuf,"Now  ");
  string = measureToString(sensorList[count].latestMeasure, " ");
  strcat(tree.sbuf, string.c_str());
  // LCD line 2
  strcat(tree.sbuf,"\nPrev ");
  string = measureToString(sensorList[count].previousMeasure, " ");
  strcat(tree.sbuf, string.c_str());
  // LCD line 3
  strcat(tree.sbuf,"\nHour ");
  string = measureToString(sensorList[count].hoursMeasure, " ");
  strcat(tree.sbuf, string.c_str());
  // LCD line 4
  strcat(tree.sbuf,"\nFirst ");
  string = yearMonthDayToString(sensorList[count].firstSampleTime);
  string += ":";
  string += hourMinuteSecondToString(sensorList[count].firstSampleTime);
  strcat(tree.sbuf, string.c_str());
  // Flush
  Serial.println(tree.sbuf);
  tree.drawUsrScreen(tree.sbuf);

}

void showSensor(int count)
{
  lcd.clear();
  showSensorRecent(count);
  Alarm.delay(displayCyclingInterval * 1000);
  showSensorHistory(count);
}

void showSensors(int unused)
{
  int count=0;
  while (count < MAX_SENSOR)
  {
    showSensor(count);
    Alarm.delay(displayCyclingInterval * 1000);
    ++count;
  }
}



void setLogMarker(int unused)
{
  Serial.println(__FUNCTION__);
  logMarker = true;
  lcd.print("Log marker set");
  Alarm.delay(tree.tm_usrScreen * 1000);
}

void clearHistory(int unused)
{
  Serial.print(__FUNCTION__);
  for(int count = 0 ; count < MAX_SENSOR ; ++count)
  {
    sensorList[count].previousMeasure.humidityRelative=0 ;
    sensorList[count].previousMeasure.humidityAbsolute=0 ;
    sensorList[count].previousMeasure.temperature=0 ;
    sensorList[count].hoursMeasure.humidityRelative = 0;
    sensorList[count].hoursMeasure.humidityAbsolute = 0;
    sensorList[count].hoursMeasure.temperature = 0;
  }
}

void setup(){
  _menu *r,*s1,*s2,*s3;
  tree.tm_usrScreen = 5;   //lap time before usrscreen  
  Serial.begin(115200);    
  setSyncProviders(RTC.get, RTC.set);
  memset(&timerList, 0, sizeof(timerList));
  // Sensor list setup
  memset(&sensorList, 0, sizeof(sensorList));

  sensorList[1].isInflowSensor = true;

  sensorList[0].digitalPin=30;
  strcpy_P(sensorList[0].description, (char *)pgm_read_word(&descriptions[0]));
  sensorList[1].digitalPin=32;
  strcpy_P(sensorList[1].description, (char *)pgm_read_word(&descriptions[1]));
  sensorList[2].digitalPin=40;
  strcpy_P(sensorList[2].description, (char *)pgm_read_word(&descriptions[2]));
  sensorList[3].digitalPin=36;
  strcpy_P(sensorList[3].description, (char *)pgm_read_word(&descriptions[3]));
  sensorList[4].digitalPin=38;
  strcpy_P(sensorList[4].description, (char *)pgm_read_word(&descriptions[4]));
  sensorList[5].digitalPin=34;
  strcpy_P(sensorList[5].description, (char *)pgm_read_word(&descriptions[5]));
/*
  sensorList[6].digitalPin=30;
  strcpy_P(sensorList[6].description, (char *)pgm_read_word(&descriptions[6]));
  sensorList[7].digitalPin=32;
  strcpy_P(sensorList[7].description, (char *)pgm_read_word(&descriptions[7]));
  sensorList[8].digitalPin=40;
  strcpy_P(sensorList[8].description, (char *)pgm_read_word(&descriptions[8]));
  sensorList[9].digitalPin=36;
  strcpy_P(sensorList[9].description, (char *)pgm_read_word(&descriptions[9]));
  sensorList[10].digitalPin=38;
  strcpy_P(sensorList[10].description, (char *)pgm_read_word(&descriptions[10]));
  sensorList[11].digitalPin=34;
  strcpy_P(sensorList[11].description, (char *)pgm_read_word(&descriptions[11]));
  sensorList[12].digitalPin=30;
  strcpy_P(sensorList[12].description, (char *)pgm_read_word(&descriptions[12]));
  sensorList[13].digitalPin=32;
  strcpy_P(sensorList[13].description, (char *)pgm_read_word(&descriptions[13]));
  sensorList[14].digitalPin=40;
  strcpy_P(sensorList[14].description, (char *)pgm_read_word(&descriptions[14]));
  sensorList[15].digitalPin=36;
  strcpy_P(sensorList[15].description, (char *)pgm_read_word(&descriptions[15]));
  sensorList[16].digitalPin=38;
  strcpy_P(sensorList[16].description, (char *)pgm_read_word(&descriptions[16]));
  sensorList[17].digitalPin=34;
  strcpy_P(sensorList[17].description, (char *)pgm_read_word(&descriptions[17]));
*/

  tree.begin(&lcd,20,4); //declare lcd object and screen size to menwiz lib

  tree.addUsrNav(navMenu, 6);
  r=tree.addMenu(MW_ROOT,NULL,F("Main menu"));
    s1=tree.addMenu(MW_VAR,r, F("Show sensors"));
    s1->addVar(MW_ACTION,showSensors, 0);
    s1->setBehaviour(MW_ACTION_CONFIRM, false);

    s1=tree.addMenu(MW_SUBMENU,r, F("Display settings"));
      s2=tree.addMenu(MW_VAR,s1,F("Show only responding"));
      s2->addVar(MW_BOOLEAN,&showOnlyRespondingSensors);
      s2=tree.addMenu(MW_VAR,s1,F("Cycling interval"));
      s2->addVar(MW_AUTO_INT, &displayCyclingInterval,1, 10, 1);
      s2=tree.addMenu(MW_VAR,s1,F("Menu timeout"));
      s2->addVar(MW_AUTO_INT, &tree.tm_usrScreen,2, 10, 1);

    s1=tree.addMenu(MW_SUBMENU,r, F("Timers"));
      s2=tree.addMenu(MW_VAR,s1, F("Restart all timers"));
      s2->addVar(MW_ACTION,timerRestartAll, 0);
      s2->setBehaviour(MW_ACTION_CONFIRM, true);

      s2=tree.addMenu(MW_VAR,s1, F("Measuring interval"));
      s2->addVar(MW_AUTO_INT, &measuringInterval,4, 60, 2);
      s2=tree.addMenu(MW_VAR,s1, F("SD log interval"));
      s2->addVar(MW_AUTO_INT, &sdLogInterval,10, 600, 10);

    s1=tree.addMenu(MW_SUBMENU,r, F("Limits / Alarms"));
      s2=tree.addMenu(MW_VAR,s1, F("Heating Temperature"));
      s2->addVar(MW_AUTO_INT, &dryingModeHeatingTemperature, -40, 80, 1);
      s2=tree.addMenu(MW_VAR,s1, F("Cooling Temperature"));
      s2->addVar(MW_AUTO_INT, &dryingModeCoolingTemperature, -40, 80, 1);
      s2=tree.addMenu(MW_VAR,s1, F("Humid. Absolute Max"));
      s2->addVar(MW_AUTO_INT, &humidityAbsoluteMax, 0, 100, 1);
      s2=tree.addMenu(MW_VAR,s1, F("Humid. Absolute Min"));
      s2->addVar(MW_AUTO_INT, &humidityAbsoluteMin, 0, 100, 1);
      s2=tree.addMenu(MW_VAR,s1, F("Humid. Relative Max"));
      s2->addVar(MW_AUTO_INT, &humidityRelativeMax, 0, 100, 1);
      s2=tree.addMenu(MW_VAR,s1, F("Humid. Relative Min"));
      s2->addVar(MW_AUTO_INT, &humidityRelativeMin, 0, 100, 1);
      s2=tree.addMenu(MW_VAR,s1, F("Temperature Max"));
      s2->addVar(MW_AUTO_INT, &temperatureMax, -40, 80, 1);
      s2=tree.addMenu(MW_VAR,s1, F("Temperature Min"));
      s2->addVar(MW_AUTO_INT, &temperatureMin, -40, 80, 1);

    s1=tree.addMenu(MW_SUBMENU,r, F("Time"));
      s2=tree.addMenu(MW_VAR,s1, F("Show time"));
      s2->addVar(MW_ACTION,showTime, 0);
      s2->setBehaviour(MW_ACTION_CONFIRM, false);
      s2=tree.addMenu(MW_SUBMENU,s1, F("Set time"));
        s3=tree.addMenu(MW_VAR, s2, F("Year Month Day"));
        s3->addVar(MW_ACTION, setYearMonthDay, 0);
        s3->setBehaviour(MW_ACTION_CONFIRM, false);

        s3=tree.addMenu(MW_VAR, s2, F("Hour Minute Second"));
        s3->addVar(MW_ACTION, setHourMinuteSecond, 0);
        s3->setBehaviour(MW_ACTION_CONFIRM, false);

    s1=tree.addMenu(MW_VAR,r, F("Show status"));
    s1->addVar(MW_ACTION,statusScreen, 0);
    s1->setBehaviour(MW_ACTION_CONFIRM, false);

    s1=tree.addMenu(MW_VAR,r, F("Clear sensor history"));
    s1->addVar(MW_ACTION,clearHistory, 0);
    s1->setBehaviour(MW_ACTION_CONFIRM, true);

    s1=tree.addMenu(MW_VAR,r, F("Set log marker"));
    s1->addVar(MW_ACTION,setLogMarker, 0);
    s1->setBehaviour(MW_ACTION_CONFIRM, true);
/*
    s1=tree.addMenu(MW_SUBMENU,r, F("Sensor calibration"));
    for(int count = 0 ; count < MAX_SENSOR ; ++count)
    {
      s2=tree.addMenu(MW_SUBMENU,s1, (const __FlashStringHelper*)pgm_read_word(&descriptions[count]));
        s3=tree.addMenu(MW_VAR, s2, F("HumidityRelative offset"));
        s3->addVar(MW_AUTO_FLOAT, &sensorList[count].humidityRelativeCalibration, -100.0, 100.0, 0.1);
        s3=tree.addMenu(MW_VAR, s2, F("Temperature offset"));
        s3->addVar(MW_AUTO_FLOAT, &sensorList[count].temperatureCalibration, -100.0, 100.0, 0.1);
        s3=tree.addMenu(MW_VAR, s2, F("Use as reference"));
        s3->addVar(MW_ACTION, calibrateAll, count);
        s3=tree.addMenu(MW_VAR, s2, F("Use as inflow sensor"));
        s3->addVar(MW_ACTION, useAsInflowSensor, count);
    }
*/
  tree.getErrorMessage(true);
  //(optional)create a user define screen callback to activate after given time since last button push 
  tree.addUsrScreen(defaultScreen,tree.tm_usrScreen);

  Serial.print(hour());
  timerSetupAll();
  Alarm.delay(2000);
}

void loop(){
  int errorCode;
  tree.draw();
  Alarm.delay(0);
}
