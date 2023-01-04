#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "arduino_secrets.h"

SoftwareSerial rs485(2, 3); // RX, TX

char ssid[] = SSID;  // SSID
char pass[] = PASSWORD;  // Password

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// MQTT 세팅
const char broker[] = MQTT_BROKER;
int port = MQTT_PORT;
const char topic_send[] = "house/cultivator_data";

// 토양 데이터 (from Sensor)
float sensor_soil_temp = 0;
float sensor_soil_humi = 0;
float sensor_soil_ec = 0;
float sensor_soil_temp_2 = 0;
float sensor_soil_humi_2 = 0;
float sensor_soil_ec_2 = 0;

// 기타 변수
unsigned long time_previous_sensing = 0;
unsigned long time_previous_publish = 0;

void setup() {
  Serial.begin(9600);     // for Serial Monitor

  // WiFi 연결
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("You're connected to the network");
  Serial.println();

  // MQTT 연결
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();
}

void loop() {
  // 연결이 끊기지 않도록 MQTT broker keep alive 전송
  mqttClient.poll();

  unsigned long time_current = millis(); // 현재 시간
  // 5초마다 토양 데이터 센싱
  if(time_current - time_previous_sensing >= 5000) {
    time_previous_sensing = time_current;
    read_soil_1();
    delay(1000);
    read_soil_2();
  }
  // 30초마다 토양 데이터 Publish
  if(time_current - time_previous_publish >= 30000) {
    time_previous_publish = time_current;
    data_publish_1();
    data_publish_2();
  }
}

// 토양 데이터 Publish
void data_publish_1() {
  StaticJsonDocument<128> doc;
  char json[64];

  doc["idx"] = 1;
  doc["soilTemp"] = sensor_soil_temp;
  doc["soilHumi"] = sensor_soil_humi;
  doc["soilEc"] = sensor_soil_ec;

  serializeJson(doc, json);

  Serial.print("Send a message with topic: ");
  Serial.println(topic_send);
  Serial.println(json);
  Serial.println();

  mqttClient.beginMessage(topic_send);
  mqttClient.print(json);
  mqttClient.endMessage();
}

// 토양 데이터 Publish
void data_publish_2() {
  StaticJsonDocument<128> doc2;
  char json[64];

  doc2["idx"] = 2;
  doc2["soilTemp"] = sensor_soil_temp_2;
  doc2["soilHumi"] = sensor_soil_humi_2;
  doc2["soilEc"] = sensor_soil_ec_2;

  serializeJson(doc2, json);

  Serial.print("Send a message with topic: ");
  Serial.println(topic_send);
  Serial.println(json);
  Serial.println();

  mqttClient.beginMessage(topic_send);
  mqttClient.print(json);
  mqttClient.endMessage();
}