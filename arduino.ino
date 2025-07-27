#include <ESP8266WiFi.h>
#define BLYNK_TEMPLATE_ID "YOUR_BLYNK_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "YOUR_BLYNK_TEMPLATE_NAME"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi Credentials
char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

// Sensor & Actuator Pins
#define DHTPIN D4
#define PIRPIN D5
#define SOIL_MOISTURE_PIN A0
#define RELAY_PIN D3
#define DHTTYPE DHT11

// Initialize Components
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD I2C Address and Size
BlynkTimer timer;

// State Variables
int pumpState = 0;
unsigned long lastManualPumpChange = 0;
const unsigned long pumpOverrideDuration = 10000; // 10 seconds

void setup() {
    Serial.begin(115200);

    // Initialize Blynk
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

    // Initialize Sensors
    dht.begin();
    pinMode(PIRPIN, INPUT);
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // Pump OFF by default

    // Initialize LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Smart Farming");

    Serial.println("System Initialized...");
    delay(2000);

    // Read sensor data every 5 seconds
    timer.setInterval(5000L, readSensors);
}

void loop() {
    Blynk.run();
    timer.run();
}

// Relay control from Blynk switch (V12)
BLYNK_WRITE(V12) {
    pumpState = param.asInt(); // 1 = ON, 0 = OFF
    digitalWrite(RELAY_PIN, pumpState);
    lastManualPumpChange = millis(); // Start 10s override
}

void readSensors() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("ERROR: Failed to read DHT sensor!");
        return;
    }

    // Soil moisture read and remap to 0–255 scale
    int rawMoisture = analogRead(SOIL_MOISTURE_PIN);
    rawMoisture = map(rawMoisture, 1024, 0, 0, 1024); // Optional linearization
    int soilMoisture = map(rawMoisture, 0, 1023, 0, 255);

    int motion = digitalRead(PIRPIN);
    String soilStatus = "";

    // Only allow auto control if 10s have passed since manual change
    if (millis() - lastManualPumpChange > pumpOverrideDuration) {
        if (soilMoisture <= 50) {
            soilStatus = "Extremely Dry - Pump ON";
            digitalWrite(RELAY_PIN, HIGH);
            pumpState = 1;
        } else if (soilMoisture > 50 && soilMoisture <= 100) {
            soilStatus = "Little Wet - Pump ON";
            digitalWrite(RELAY_PIN, HIGH);
            pumpState = 1;
        } else if (soilMoisture > 100 && soilMoisture <= 200) {
            soilStatus = "Normal Wet - Pump OFF";
            digitalWrite(RELAY_PIN, LOW);
            pumpState = 0;
        } else {
            soilStatus = "Extremely Wet - Pump OFF";
            digitalWrite(RELAY_PIN, LOW);
            pumpState = 0;
        }
    } else {
        soilStatus = "Manual Override Active";
    }

    // Serial Monitor Output
    Serial.println("------ SENSOR DATA ------");
    Serial.print("Temperature: "); Serial.print(temperature, 1); Serial.println(" °C");
    Serial.print("Humidity: "); Serial.print(humidity, 1); Serial.println(" %");
    Serial.print("Soil Moisture (0–255): "); Serial.print(soilMoisture); Serial.print(" - "); Serial.println(soilStatus);
    Serial.print("Motion Detected: "); Serial.println(motion ? "YES" : "NO");
    Serial.print("Pump Status: "); Serial.println(pumpState ? "ON" : "OFF");
    Serial.println("-------------------------\n");

    // LCD Display
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    lcd.print("T:"); lcd.print(temperature, 1); lcd.print("C ");
    lcd.setCursor(9, 0);
    lcd.print("H:"); lcd.print(humidity, 1); lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print("S.M:"); lcd.print(soilMoisture);
    lcd.setCursor(10, 1);
    lcd.print(pumpState ? "P:ON" : "P:OFF");

    // Send to Blynk
    Blynk.virtualWrite(V0, temperature);
    Blynk.virtualWrite(V1, humidity);
    Blynk.virtualWrite(V3, soilMoisture);
    Blynk.virtualWrite(V12, pumpState);
}