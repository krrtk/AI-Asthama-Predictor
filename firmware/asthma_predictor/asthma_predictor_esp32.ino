#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include "MAX30105.h"

// ---------------- WIFI ----------------
const char* ssid       = "YOUR NETWORK NAME";
const char* password   = "YOUR PASSWORD";
const char* serverName = "ENTER YOUR SERVER NAME";

// ---------------- PINS ----------------
#define MQ135_PIN  4
#define DHT_PIN    5
#define DHT_TYPE   DHT11
#define BUZZER_PIN 18
#define I2C_SDA    8
#define I2C_SCL    9

// ---------------- OBJECTS ----------------
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
MAX30105 particleSensor;

// ---------------- CONSTANTS ----------------
#define FINGER_THRESHOLD   30000 // Lowered slightly for better detection
#define DC_FILTER_ALPHA    0.95  
#define AC_BEAT_THRESHOLD  200   // Lowered to catch "weaker" pulses
#define MIN_BEAT_MS        450   
#define MAX_BEAT_MS        1200  
#define RATE_SIZE          4     // Smaller buffer for faster initial reading

// ---------------- STATES ----------------
float dcValue=0, prevAC=0, dcRed=0;
bool wasNegative=false;
long lastBeatTime=0;
float rates[RATE_SIZE];
byte rateSpot=0;
int validCount=0;
float avgBpm=0, bpm=0, spo2=0;
float acRedSumSq=0, acIRSumSq=0;
int spo2Samples=0;
bool spo2Ready=false;

void resetMAX() {
  dcValue=0; prevAC=0; wasNegative=false;
  lastBeatTime=0; rateSpot=0; validCount=0;
  avgBpm=0; bpm=0; dcRed=0;
  acRedSumSq=0; acIRSumSq=0;
  spo2Samples=0; spo2=0;
  spo2Ready=false;
}

void sampleMAX30102(unsigned long durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    long irRaw  = particleSensor.getIR();
    long redRaw = particleSensor.getRed();

    if (irRaw < FINGER_THRESHOLD) {
      resetMAX();
      delay(10);
      continue;
    }

    if (dcValue == 0) { dcValue = irRaw; dcRed = redRaw; }
    dcValue = DC_FILTER_ALPHA * dcValue + (1 - DC_FILTER_ALPHA) * irRaw;
    dcRed   = DC_FILTER_ALPHA * dcRed   + (1 - DC_FILTER_ALPHA) * redRaw;

    float acIR  = irRaw  - dcValue;
    float acRed = redRaw - dcRed;

    // Heart Rate Detection
    if (wasNegative && acIR > 0) { // Zero-crossing detection
       long now = millis();
       long delta = now - lastBeatTime;
       if (delta > MIN_BEAT_MS && delta < MAX_BEAT_MS) {
         bpm = 60000.0 / delta;
         rates[rateSpot++] = bpm;
         rateSpot %= RATE_SIZE;
         if (validCount < RATE_SIZE) validCount++;
         
         float sum = 0;
         for(int i=0; i<validCount; i++) sum += rates[i];
         avgBpm = sum / validCount;
       }
       lastBeatTime = now;
       wasNegative = false;
    }
    if (acIR < -AC_BEAT_THRESHOLD) wasNegative = true;

    // SpO2 Logic
    acRedSumSq += acRed * acRed;
    acIRSumSq  += acIR * acIR;
    spo2Samples++;
    if (spo2Samples >= 50) {
      float rmsRed = sqrt(acRedSumSq/50);
      float rmsIR = sqrt(acIRSumSq/50);
      if (rmsIR > 0) {
        float R = (rmsRed/dcRed)/(rmsIR/dcValue);
        spo2 = constrain(110.0 - 25.0 * R, 90.0, 100.0);
        spo2Ready = true;
      }
      acRedSumSq=0; acIRSumSq=0; spo2Samples=0;
    }
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  dht.begin();
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init(); lcd.backlight();

  // --- WiFi Connecting Screen ---
  lcd.clear();
  lcd.print("WiFi: Connecting");
  lcd.setCursor(0,1);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
  }

  // --- Connected Screen ---
  lcd.clear();
  lcd.print("WiFi Connected!");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());
  delay(3000);

  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    lcd.clear(); lcd.print("MAX ERROR");
    while (1);
  }
  particleSensor.setup(255, 1, 2, 800, 411, 16384);
}

void loop() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Reading pulse...");
  lcd.setCursor(0,1); lcd.print("Place finger");

  sampleMAX30102(8000);

  int hr = (validCount > 0) ? (int)avgBpm : 0;
  int s2 = (spo2Ready) ? (int)spo2 : 0;
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  lcd.clear();
  lcd.print("T:" + String(t,1) + " H:" + String(h,0));
  lcd.setCursor(0,1);
  lcd.print("HR:" + String(hr) + " SpO2:" + String(s2) + "%");

  // API Call
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");
    StaticJsonDocument<200> doc;
    doc["air_quality"] = analogRead(MQ135_PIN);
    doc["temperature"] = isnan(t) ? 0 : t;
    doc["humidity"] = isnan(h) ? 0 : h;
    doc["heart_rate"] = hr;
    doc["spo2"] = s2;
    String json;
    serializeJson(doc, json);
    http.POST(json);
    http.end();
  }
  delay(3000);
}