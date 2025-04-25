#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define ANALOG_PIN 35
#define BUTTON_PIN 2
#define VOLTAGE_MAX 3.3
#define VOLTAGE_MIN 1.55

WiFiClient espClient;
PubSubClient client(espClient);

// MQTT server settings
const char* mqtt_server = "160.187.97.115";
const int mqtt_port = 1883;
const char* mqtt_user = "roboshoptech";
const char* mqtt_pass = "roboshoptech1234";
const char* topicRoot = "sensor/analog_percentage";

// Sleep config: 30 minutes
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 1800

// CUSTOM PRODUCT ID 
String productID = "RoboshopMakesense_0002";

// Map float values
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Stable analog read
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

// Enter deep sleep
void goToDeepSleep() {
  Serial.println("Going to deep sleep for 30 minutes...");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Use product ID as AP name
  String apName = productID;

  // WiFiManager setup
  WiFiManager wm;
  bool res;

  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Button pressed - starting config portal");
    wm.resetSettings(); // Optional: clear previous WiFi
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

  // MQTT connect
  client.setServer(mqtt_server, mqtt_port);
  reconnectMQTT();

  // Read analog and calculate
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

  // Create JSON payload
  StaticJsonDocument<256> doc;
  doc["device_id"] = productID;
  doc["voltage"] = voltage;
  doc["percentage"] = percentage;

  char payload[128];
  serializeJson(doc, payload);

  // Compose MQTT topic
  String fullTopic = String(topicRoot) + "/" + productID;

  // Debug output
  Serial.print("Publishing to topic: ");
  Serial.println(fullTopic);
  Serial.print("Payload: ");
  Serial.println(payload);

  // Send MQTT message
  client.publish(fullTopic.c_str(), payload);
  delay(1000);

  client.disconnect();
  WiFi.disconnect(true);

  // Sleep
  goToDeepSleep();
}

void loop() {
  // Not used
}
