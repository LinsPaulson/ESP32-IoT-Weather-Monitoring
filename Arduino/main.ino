/*
 * ESP32 ThingsBoard IoT Temperature Monitoring
 * Hardware: ESP32 + DHT11
 * Features:
 * - WiFi connectivity
 * - MQTT communication with ThingsBoard
 * - Telemetry data collection
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// WiFi Configuration
const char* ssid = "Your_SSID";
const char* password = "Your_PSWD";

// ThingsBoard Configuration
const char* thingsboardServer = "demo.thingsboard.io"; // Changed to correct endpoint
const char* mqttUser  = "Your_Token"; // Device access token from ThingsBoard
const int mqttPort = 1883;

// Sensor Configuration
#define DHTPIN 4        // GPIO pin connected to DHT11
#define DHTTYPE DHT11   // DHT 11 sensor type

// Global Objects
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  connectToWiFi();
  connectToThingsBoard();
  
  float temperature = round(dht.readTemperature());
  float humidity = round(dht.readHumidity());
  
  if (!isnan(temperature) && !isnan(humidity)) {
    sendTelemetry(temperature, humidity);
  } else {
    Serial.println("Failed to read from DHT sensor!");
  }
}

void loop() {
  // You can add a delay or a mechanism to periodically send data
  delay(5000); // Delay for 60 seconds before the next reading
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  if (!isnan(temperature) && !isnan(humidity)) {
    sendTelemetry(temperature, humidity);
  } else {
    Serial.println("Failed to read from DHT sensor!");
  }
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void connectToThingsBoard() {
  client.setServer(thingsboardServer, mqttPort);
  
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard...");
    
    if (client.connect("ESP32_Client", mqttUser , NULL)) { // NULL as password
      Serial.println("Connected!");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void sendTelemetry(float temp, float hum) {
  // Create JSON payload
  String payload = "{";
  payload += "\"temperature\":"; payload += temp; payload += ",";
  payload += "\"humidity\":"; payload += hum;
  payload += "}";
  
  // Publish to telemetry topic
  if (client.publish("v1/devices/me/telemetry", payload.c_str())) {
    Serial.println("Telemetry published successfully");
    Serial.println(payload);
  } else {
    Serial.println("Failed to publish telemetry");
  }
  
  // Small delay to ensure message is sent
  delay(100);
}
