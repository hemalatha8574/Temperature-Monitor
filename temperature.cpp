/*
  Project: Temperature Monitoring (LM35)
  Features:
  - Reads LM35 on A0, converts to °C and °F
  - Moving average filter
  - Optional I2C 16x2 LCD display (toggle via USE_LCD)
  - High-temperature alarm with buzzer
  - Two buttons to increase/decrease alarm threshold
  - CSV logging over Serial for plotting
*/

#include <Wire.h>
#include <EEPROM.h>

// Uncomment the next line if you have LiquidCrystal_I2C installed.
// #include <LiquidCrystal_I2C.h>

// ------------------ USER CONFIG ------------------
const bool USE_LCD = false;          // set true if you have LCD
// LiquidCrystal_I2C lcd(0x27,16,2); // typical address 0x27 or 0x3F

const uint8_t PIN_LM35 = A0;
const uint8_t PIN_BUZZER = 9;        // passive buzzer
const uint8_t PIN_BTN_UP = 2;
const uint8_t PIN_BTN_DN = 3;

const uint8_t AVG_WIN = 10;
const uint32_t SAMPLE_MS = 250;
const uint32_t LCD_MS = 500;

float alarmC = 35.0;                 // default alarm threshold °C
const float STEP = 0.5;

const int EE_MAGIC = 0;
const int EE_ALARM = 2;
const uint16_t MAGIC = 0x1234;
// -------------------------------------------------

volatile bool upPressed=false, dnPressed=false;
uint32_t tSample=0, tLcd=0;
float buf[20]; uint8_t head=0, cnt=0;

void isrUp() { upPressed = true; }
void isrDn() { dnPressed = true; }

void beepon(uint16_t ms) {
  tone(PIN_BUZZER, 2000); delay(ms); noTone(PIN_BUZZER);
}

float readCelsius() {
  int raw = analogRead(PIN_LM35);
  float voltage = (raw * 5.0) / 1023.0; // assuming 5V ref
  float c = voltage * 100.0;            // LM35: 10mV/°C
  return c;
}

float smooth(float v) {
  buf[head]=v;
  head=(head+1)%AVG_WIN;
  if (cnt<AVG_WIN) cnt++;
  double s=0; for (uint8_t i=0;i<cnt;i++) s+=buf[i];
  return s / cnt;
}

void loadCfg() {
  uint16_t m; EEPROM.get(EE_MAGIC, m);
  if (m==MAGIC) EEPROM.get(EE_ALARM, alarmC);
}
void saveCfg() {
  EEPROM.put(EE_MAGIC, MAGIC);
  EEPROM.put(EE_ALARM, alarmC);
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_UP), isrUp, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_DN), isrDn, FALLING);

  if (USE_LCD) {
    // lcd.init(); lcd.backlight();
    // lcd.print("Temp Monitor");
  }

  loadCfg();
  Serial.println(F("time_ms,rawC,avgC,alarmC,alarm"));
}

void loop() {
  uint32_t now = millis();

  if (upPressed) { upPressed=false; alarmC += STEP; saveCfg(); beepon(80); }
  if (dnPressed) { dnPressed=false; alarmC -= STEP; if (alarmC< -10) alarmC=-10; saveCfg(); beepon(80); }

  static float lastRaw=0, lastAvg=0;
  if (now - tSample >= SAMPLE_MS) {
    tSample = now;
    lastRaw = readCelsius();
    lastAvg = smooth(lastRaw);

    bool alarm = lastAvg >= alarmC;
    if (alarm) { tone(PIN_BUZZER, 1800); } else { noTone(PIN_BUZZER); }

    // CSV log
    Serial.print(now); Serial.print(',');
    Serial.print(lastRaw,2); Serial.print(',');
    Serial.print(lastAvg,2); Serial.print(',');
    Serial.print(alarmC,2); Serial.print(',');
    Serial.println(alarm ? 1 : 0);
  }

  if (USE_LCD && now - tLcd >= LCD_MS) {
    tLcd = now;
    // lcd.clear();
    // lcd.setCursor(0,0); lcd.print("T:"); lcd.print(lastAvg,1); lcd.print((char)223); lcd.print("C");
    // lcd.setCursor(0,1); lcd.print("Alarm:"); lcd.print(alarmC,1); lcd.print("C ");
  }
}
