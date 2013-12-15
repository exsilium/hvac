/*
  HVAC 0.0.1
 */

// LOOP_DELAY defines the delay in milliseconds between the main loop execution. 1000 = 1 sec.
#define LOOP_DELAY 1000
// SAVE_LOOP defines the amount of loops that needs to be processed before writing new values to EEPROM
#define SAVE_LOOP 4000
// STARTUP_LOOP defines the number of loop counts before the switching logic kicks in for switching on the heating.
// This is to accomodate the digital lowpass filter and incorrent temperature reading when we are starting up the controller
#define STARTUP_LOOP 60
 
// Pin 13 has an LED connected on most Arduino boards.
int led = 13;
int saveLoop = 0;
int startupLoop = 0;

boolean controller_enabled = true;
double sensorValue = 0;

#include <EEPROM.h>
#define CONFIG_VERSION "HVAC1"
#define CONFIG_START 32

#include <RTDModule.h>
RTDModule rtd;

struct StoreStruct {
  // The variables of your settings
  float setLowLimit, setHighLimit, setAlertLimit;
  // This is for mere detection if they are your settings
  char version_of_program[6]; // it is the last variable of the struct
  // so when settings are saved, they will only be validated if
  // they are stored completely.
} settings = {
  // The default values
  25.00, 50.00, 90.00,
  CONFIG_VERSION
};

float lowLimit = 0.00;
float highLimit = 0.00;
uint8_t switchState = LOW;

// include the library code:
#include <LiquidCrystal.h>
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

void loadConfig() {
  // To make sure there are settings, and they are YOURS!
  // If nothing is found it will use the default settings.
  if (EEPROM.read(CONFIG_START + sizeof(settings) - 2) == settings.version_of_program[4] &&
      EEPROM.read(CONFIG_START + sizeof(settings) - 3) == settings.version_of_program[3] &&
      EEPROM.read(CONFIG_START + sizeof(settings) - 4) == settings.version_of_program[2] &&
      EEPROM.read(CONFIG_START + sizeof(settings) - 5) == settings.version_of_program[1] &&
      EEPROM.read(CONFIG_START + sizeof(settings) - 6) == settings.version_of_program[0])
  { // reads settings from EEPROM
    for (unsigned int t=0; t<sizeof(settings); t++)
      *((char*)&settings + t) = EEPROM.read(CONFIG_START + t);
  } else {
    // settings aren't valid! will overwrite with default settings
    saveConfig();
  }
}

void saveConfig() {
  for (unsigned int t=0; t<sizeof(settings); t++)
  { // writes to EEPROM
    EEPROM.write(CONFIG_START + t, *((char*)&settings + t));
    // and verifies the data
    if (EEPROM.read(CONFIG_START + t) != *((char*)&settings + t))
    {
      Serial.println("Error saving settings to EEPROM!");
    }
  }
}

//Digital low pass filter
float digitalLowPass(float last_smoothed, float new_value, float filterVal)
{
  float smoothed = (new_value * (1.00 - filterVal)) + (last_smoothed * filterVal);
  return smoothed;
}

// the setup routine runs once when you press reset:
void setup() {
  pinMode(led, OUTPUT);
  digitalWrite(led, switchState);
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  lcd.print("HiL: ");
  lcd.setCursor(0, 1);
  lcd.print("LoL: ");
  
  loadConfig();
  lowLimit = settings.setLowLimit;
  highLimit = settings.setHighLimit;
  
  lcd.setCursor(4, 0);
  lcd.print(highLimit, 1);
  lcd.setCursor(4, 1);
  lcd.print(lowLimit, 1);
  
  rtd.setPins(8,9,0); // We don't use a multiplexer, but the pins are still set accordingly
  rtd.calibration(0, 0.135783, -7.74602);
  
  analogReference(INTERNAL); // if we need to switch to 1.1v reference
}

// the loop routine runs over and over again forever:
void loop() {
  sensorValue = digitalLowPass(sensorValue, rtd.getTemperature(0), 0.90);
  int dialValue = analogRead(A1) / 12;

  uint8_t lowLimitChange = digitalRead(7);
  uint8_t highLimitChange = digitalRead(6);
    
  if(controller_enabled) { // main operations happens here, controller_enabled = true start
    Serial.println(analogRead(A0));
    Serial.println(sensorValue);
    lcd.setCursor(14,0);
    lcd.print(" ");
    lcd.setCursor(9,0);
    lcd.print("T");
    lcd.print(sensorValue, 1);
    
    // Switching Logic
    if(sensorValue < lowLimit && sensorValue < highLimit && highLimit > 0.00 && switchState == LOW && startupLoop >= STARTUP_LOOP) {
      Serial.println("Switching on Heating");
      switchState = HIGH;
    
      lcd.setCursor(15,0);
      lcd.print("O");
      lcd.setCursor(15,1);
      lcd.print("N");
    }
    else if((sensorValue >= highLimit || highLimit == 0.00) && switchState == HIGH) {
      // explicit check against highLimit 0.00 means we will not trigger heating even when going below 0
      Serial.println("Switching off Heating");
      switchState = LOW;
    
      lcd.setCursor(15,0);
      lcd.print(" ");
      lcd.setCursor(15,1);
      lcd.print(" ");
    }
    if(sensorValue >= settings.setAlertLimit) {
      Serial.println("ALARM!!!!!!");
      lcd.setCursor(9,1);
      lcd.print("ALARM");
      switchState = LOW; 
    }
    // Switching Logic end
    digitalWrite(led, switchState);
  
    // Do we need to save or just increment the saveLoop?
    if((lowLimit != settings.setLowLimit || highLimit != settings.setHighLimit) && saveLoop > 0) {
      if(saveLoop < SAVE_LOOP) {
        saveLoop++;
      }
      else {
        settings.setLowLimit = lowLimit;
        settings.setHighLimit = highLimit;
        Serial.println("Saving new values");
        saveConfig();
        saveLoop = 0; 
      }
    }
  } // controller_enabled = true end
  else {
    if(lowLimitChange == HIGH) {
      lowLimit = dialValue;
      saveLoop = 1;
    }
    else if(highLimitChange == HIGH) {
      highLimit = dialValue; 
      saveLoop = 1;
    }
    // set the cursor to column 6, line 0
    lcd.setCursor(4, 0);
    lcd.print(highLimit, 1);
    lcd.setCursor(4, 1);
    lcd.print(lowLimit, 1);
  } // controller_enabled = false end
  
  if(lowLimitChange == HIGH || highLimitChange == HIGH) {
    controller_enabled = false;
    analogReference(DEFAULT);
    
    lcd.setCursor(9,0);
    lcd.print(" !!!!!");
    lcd.setCursor(10,1);
    lcd.print("!!!!!");
  }
  else if (!controller_enabled) {
    // This doesn't work in practise, the readings are still 10C off/lower when
    // the controller is enabled - needs to be fixed because it currently
    // triggers the switch incorrectly.
    lcd.setCursor(10,0);
    analogReference(INTERNAL);
    sensorValue = digitalLowPass(sensorValue, rtd.getTemperature(0), 0.90);
    lcd.print(" ");
    delay(1000);
    sensorValue = digitalLowPass(sensorValue, rtd.getTemperature(0), 0.90);
    lcd.print(" ");
    delay(1000);
    sensorValue = digitalLowPass(sensorValue, rtd.getTemperature(0), 0.90);
    lcd.print(" ");
    delay(1000);
    sensorValue = digitalLowPass(sensorValue, rtd.getTemperature(0), 0.90);
    lcd.print(" ");
    delay(1000);
    sensorValue = digitalLowPass(sensorValue, rtd.getTemperature(0), 0.90);
    lcd.print(" ");
    lcd.setCursor(10,1);
    delay(1000);
    sensorValue = digitalLowPass(sensorValue, rtd.getTemperature(0), 0.90);
    lcd.print(" ");
    delay(1000);
    sensorValue = digitalLowPass(sensorValue, rtd.getTemperature(0), 0.90);
    lcd.print(" ");
    delay(1000);
    sensorValue = digitalLowPass(sensorValue, rtd.getTemperature(0), 0.90);
    lcd.print(" ");
    delay(1000);
    sensorValue = digitalLowPass(sensorValue, rtd.getTemperature(0), 0.90);
    lcd.print(" ");
    delay(1000);
    sensorValue = digitalLowPass(sensorValue, rtd.getTemperature(0), 0.90);
    lcd.print(" ");
    controller_enabled = true;
  }
 
  if(startupLoop < STARTUP_LOOP) {
    startupLoop++; 
  }
 
  delay(LOOP_DELAY);  // wait
}
