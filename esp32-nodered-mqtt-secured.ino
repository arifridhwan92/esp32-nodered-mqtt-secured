#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define ANALOG_PIN 35
#define BUTTON_PIN 2
#define VOLTAGE_MAX 3.3
#define VOLTAGE_MIN 1.55

RTC_DATA_ATTR int bootCount = 0;

WiFiClient espClient;
PubSubClient client(espClient);

// MQTT credentials
const char* mqtt_server = "160.187.97.115";
const int mqtt_port = 1883;
const char* mqtt_user = "roboshoptech";
const char* mqtt_pass = "roboshoptech1234";
const char* topic = "sensor/analog_percentage/0001";

// Sleep time: 30 minutes (in microseconds)
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 1800 // seconds (30 min)

// ===== Custom Device ID for AP Name =====
String deviceID = "0001"; // Change manually for each device
String apName = "RoboshopMakesense_" + deviceID;

// Map float values
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Stable analog read with averaging
int readStableAnalog(int pin, int samples = 10) {
  long total = 0;
  for (int i = 0; i < samples; i++) {
    total += analogRead(pin);
    delay(2);
  }
  return total / samples;
}

// MQTT reconnect
void reconnectMQTT() {
  int attempts = 0;
  while (!client.connected() && attempts < 10) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying...");
      delay(1000);
      attempts++;
    }
  }
}

void goToDeepSleep() {
  Serial.println("Going to sleep for 30 minutes...");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  // WiFi setup
  WiFiManager wm;
  bool res;

  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Button pressed - starting WiFi config portal");
    wm.resetSettings();
    res = wm.startConfigPortal(apName.c_str());
    if (!res) {
      Serial.println("Failed to connect. Restarting...");
      ESP.restart();
    }
  } else {
    wm.autoConnect(apName.c_str());
  }

  Serial.println("WiFi connected. IP:");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, mqtt_port);
  reconnectMQTT();

  // Read analog and calculate voltage and percentage
  int adcValue = readStableAnalog(ANALOG_PIN);
  float voltage = adcValue * (VOLTAGE_MAX / 4095.0);
  float percentage;

  if (voltage >= VOLTAGE_MAX) {
    percentage = 0.0;
  } else if (voltage <= VOLTAGE_MIN) {
    percentage = 100.0;
  } else {
    percentage = mapFloat(voltage, VOLTAGE_MAX, VOLTAGE_MIN, 0.0, 100.0);
  }

  // Create JSON object
  StaticJsonDocument<200> doc;
  doc["device"] = apName;
  doc["soil_moisture"] = percentage;
  doc["voltage"] = voltage;
  
  

  // Serialize JSON to string
  char payload[128];
  serializeJson(doc, payload);

  // Debug print
  Serial.print("Publishing JSON: ");
  Serial.println(payload);

  // Publish JSON payload
  client.publish(topic, payload);

  delay(1000); // Allow MQTT send time
  client.disconnect();
  WiFi.disconnect(true);
  goToDeepSleep();
}

void loop() {
  // Not used – deep sleep handles everything
}
