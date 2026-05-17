#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include "pest_model.h"

#define DHT_PIN   5
#define DHT_TYPE  DHT22
#define LDR_PIN   4
#define LED_HIGH  15
#define LED_MED   16
#define LED_LOW   18

const char* WIFI_SSID          = "Wokwi-GUEST";
const char* WIFI_PASSWORD      = "";
const char* THINGSPEAK_API_KEY = "4R45G1ZDU456E23D";
const char* THINGSPEAK_URL     = "http://api.thingspeak.com/update";

DHT dht(DHT_PIN, DHT_TYPE);
Eloquent::ML::Port::PestRiskClassifier classifier;

const char* RISK_LABELS[] = { "High", "Low", "Medium" };

float readSunlight() {
  return (analogRead(LDR_PIN) / 4095.0f) * 12.0f;
}

float computeTHI(float temp, float humidity) {
  return (temp * humidity) / 100.0f;
}

void buildFeatures(float* f, float temp, float humidity, float sunlight) {
  f[0] = temp;
  f[1] = humidity;
  f[2] = sunlight;
  f[3] = 15.0f;
  f[4] = 400.0f;
  f[5] = computeTHI(temp, humidity);
}

void setLEDs(int prediction) {
  digitalWrite(LED_HIGH, prediction == 0);
  digitalWrite(LED_MED,  prediction == 2);
  digitalWrite(LED_LOW,  prediction == 1);
}

bool uploadToThingSpeak(float temp, float humidity, float sunlight, float thi, int prediction) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Not connected!");
    return false;
  }
  HTTPClient http;
  String url = String(THINGSPEAK_URL) +
               "?api_key=" + THINGSPEAK_API_KEY +
               "&field1=" + String(temp, 1) +
               "&field2=" + String(humidity, 1) +
               "&field3=" + String(sunlight, 1) +
               "&field4=" + String(thi, 2) +
               "&field5=" + String(prediction);
  http.begin(url);
  int code = http.GET();
  http.end();
  if (code == 200) { Serial.println("[ThingSpeak] Upload OK"); return true; }
  Serial.print("[ThingSpeak] Error: "); Serial.println(code);
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_HIGH, OUTPUT);
  pinMode(LED_MED,  OUTPUT);
  pinMode(LED_LOW,  OUTPUT);
  digitalWrite(LED_HIGH, LOW);
  digitalWrite(LED_MED,  LOW);
  digitalWrite(LED_LOW,  LOW);

  dht.begin();
  analogReadResolution(12);

  Serial.println("=========================================");
  Serial.println("   AIoT Pest Risk Detection System");
  Serial.println("   ESP32-S3 + DHT22 + LDR -> ThingSpeak");
  Serial.println("=========================================\n");

  Serial.print("[WiFi] Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] Offline mode");
  }
  Serial.println("[System] Ready - sampling every 30s\n");
}

void loop() {
  Serial.println("---- Sensor Reading ----");

  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();
  float sunlight    = readSunlight();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("[DHT22] Read error. Retry in 2s...");
    delay(2000);
    return;
  }

  float thi = computeTHI(temperature, humidity);

  Serial.printf("[DHT22]  Temp: %.1fC  |  Humidity: %.1f%%\n", temperature, humidity);
  Serial.printf("[LDR]    Sunlight: %.1f hrs/day\n", sunlight);
  Serial.printf("[Calc]   THI: %.2f\n", thi);

  float features[6];
  buildFeatures(features, temperature, humidity, sunlight);

  int prediction = classifier.predict(features);
  Serial.printf("[ML] Pest Risk: %s\n", RISK_LABELS[prediction]);

  setLEDs(prediction);

  Serial.print("[Alert] ");
  if (prediction == 0)      Serial.println("HIGH RISK - Inspect crops!");
  else if (prediction == 2) Serial.println("MEDIUM RISK - Monitor closely.");
  else                      Serial.println("LOW RISK - Conditions safe.");

  Serial.println("[Cloud] Uploading to ThingSpeak...");
  uploadToThingSpeak(temperature, humidity, sunlight, thi, prediction);

  Serial.println("---- Done. Next in 30s ----\n");
  delay(30000);
}