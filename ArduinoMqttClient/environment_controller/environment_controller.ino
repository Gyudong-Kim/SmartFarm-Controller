#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "arduino_secrets.h"

#define INFO
#include "PinDefinitionsAndMore.h"
#include <IRremote.hpp>
#include "ac_LG.hpp"

SoftwareSerial rs485(2, 3); // RX, TX
Aircondition_LG MyLG_Aircondition;

// 핀 번호 지정
#define INTAKE 8
#define EXHAUST 9
#define HUMIDIFIER 10

char ssid[] = SSID;  // SSID
char pass[] = PASSWORD; // Password

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// MQTT 세팅
const char broker[] = MQTT_BROKER;
int port = MQTT_PORT;
const char topic_receive[] = "environment_control";
const char topic_send[] = "environment_status";
const char topic_send_2[] = "house/environment_data";

// 환경 제어기 설정 변수
int set_temp = 0;
int set_humi = 0;
int set_co2 = 0;

// 환경 데이터 (from Sensor)
float sensor_temp = 0;
float sensor_humi = 0;
float sensor_co2 = 0;

// 기타 변수
unsigned long time_previous_sensing = 0;
unsigned long time_previous_publish = 0;
int C_M_005_ing = 0;
int fan_ing = 0;
int humidifier_ing = 0;

void setup() {
  Serial.begin(9600);          // for Serial Monitor

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(INTAKE, OUTPUT);     // 흡기팬
  pinMode(EXHAUST, OUTPUT);    // 배기팬
  pinMode(HUMIDIFIER, OUTPUT); // 가습기

  // LG 에어컨 IR LED 세팅
  IrSender.begin();
  Serial.print("Ready to send IR signals at Pin: ");
  Serial.println(IR_SEND_PIN);
  MyLG_Aircondition.setType(LG_IS_WALL_TYPE);
  Serial.println();

  // WiFi 연결
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(500);
  }

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

  // MQTT 메시지 수신 콜백 설정
  mqttClient.onMessage(onMqttMessage);

  // Subscribe 토픽
  Serial.print("Subscribing to topic: ");
  Serial.println(topic_receive);

  mqttClient.subscribe(topic_receive);

  Serial.println();
}

void loop() {
  // 연결이 끊기지 않도록 MQTT broker keep alive 전송
  mqttClient.poll();

  unsigned long time_current = millis(); // 현재 시간
  // 5초마다 환경 데이터 센싱
  if(time_current - time_previous_sensing >= 5000) {
    time_previous_sensing = time_current;
    read_co2();
    delay(1000);
    read_temp_humi();
  }
  // 30초마다 환경 및 상태 데이터 Publish
  if(time_current - time_previous_publish >= 30000) {
    time_previous_publish = time_current;
    data_publish();
    status_publish();
  }

  // 환경 제어기가 작동 중일 때
  if(C_M_005_ing == 1) {
    // 흡/배기팬 작동
    if(sensor_co2 - set_co2 >= 300) {         // 현재 이산화탄소 농도가 설정 값보다 500ppm 이상 높다면 흡기팬 작동
      if(fan_ing == 0) {
        digitalWrite(INTAKE, HIGH);

        fan_ing = 1;

        status_publish();
        Serial.println("===> Intake Fan Start");
        Serial.println();
      }
    } else if (sensor_co2 - set_co2 <= -300) { // 현재 이산화탄소 농도가 설정 값보다 300ppm 이상 낮다면 배기팬 작동
      if(fan_ing == 0) {
        digitalWrite(EXHAUST, HIGH);

        fan_ing = 1;

        status_publish();
        Serial.println("===> Exhaust Fan Start");
        Serial.println();
      }
    } else {                                  // 현재 이산화탄소 농도와 설정 값의 차가 300ppm 미만일 때 흡/배기팬 정지
      if(fan_ing == 1) {
        digitalWrite(INTAKE, LOW);
        digitalWrite(EXHAUST, LOW);

        fan_ing = 0;

        status_publish();
        Serial.println("===> Fan Stop");
        Serial.println();
      }
    }

    // 가습기 작동
    if(sensor_humi - set_humi <= -5) {        // 현재 습도가 설정 값보다 5% 이상 낮다면 가습기 작동
      if(humidifier_ing == 0) {
        digitalWrite(HUMIDIFIER, HIGH);

        humidifier_ing = 1;

        status_publish();
        Serial.println("===> Humidifier Start");
        Serial.println();
      }
    } else {                                  // 현재 습도와 설정 값의 차이가 5% 미만일 때 가습기 정지
      if(humidifier_ing == 1) {
        digitalWrite(HUMIDIFIER, LOW);

        humidifier_ing = 0;

        status_publish();
        Serial.println("===> Humidifier Stop");
        Serial.println();
      }
    }
  }
}

// MQTT 메시지 수신
void onMqttMessage() {
  String buffer;

  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.println("'");

  // 수신된 메시지 처리
  while (mqttClient.available()) {
    char data = mqttClient.read();
    if(data == '\n') {
      break;
    } else {
      buffer = buffer + data;
    }
    delay(10);
  }
  Serial.println(buffer);
  Serial.println();
  set_control(buffer);
}

// 환경 제어기 작동 세팅
void set_control(String message) {
  StaticJsonDocument<64> doc;
  deserializeJson(doc, message); // 문자열을 JSON형식으로 변환

  if(doc["code"]=="C_M_005") { // OP_CODE check
    set_temp = doc["temp"];
    set_humi = doc["humi"];
    set_co2 = doc["co2"];

    if(set_temp == 0 && set_humi == 0 && set_co2 == 0) { // 환경 제어기 작동정지
      digitalWrite(INTAKE, LOW);
      digitalWrite(EXHAUST, LOW);
      digitalWrite(HUMIDIFIER, LOW);

      C_M_005_ing = 0;
      humidifier_ing = 0;
      fan_ing = 0;

      air_conditioner_control();

      status_publish();
      Serial.println("Save setting values");
      Serial.println();
      Serial.println("===> Environment Controller stop");
      Serial.println();
    } else {
      C_M_005_ing = 1;

      air_conditioner_control();
      
      Serial.println("Save setting values");
      Serial.println();
      Serial.println("===> Environment Controller start");
      Serial.println();
    }
  } else {
    Serial.println("Received the wrong message");
    Serial.println();
  }
}

void air_conditioner_control() {
  if(set_temp == 0) {
    MyLG_Aircondition.sendCommandAndParameter('0', 0);
    
    status_publish();
    Serial.println("===> Air Conditioner stop");
    Serial.println();
  } else {
    MyLG_Aircondition.sendCommandAndParameter('1', 0);
    delay(1000);
    MyLG_Aircondition.sendCommandAndParameter('m', 49);
    delay(1000);
    MyLG_Aircondition.sendCommandAndParameter('t', set_temp);

    status_publish();
    Serial.println("===> Air Conditioner Start");
    Serial.println();
  }
}

// 환경 제어기 작동상태 Publish
void status_publish() {
  StaticJsonDocument<64> doc2;
  char json[64];

  doc2["air_conditioner"] = set_temp;
  doc2["humidifier"] = humidifier_ing;
  doc2["fan"] = fan_ing;

  serializeJson(doc2, json);

  Serial.print("Send a message with topic: ");
  Serial.println(topic_send);
  Serial.println(json);
  Serial.println();

  mqttClient.beginMessage(topic_send);
  mqttClient.print(json);
  mqttClient.endMessage();
}

// 환경 데이터 Publish
void data_publish() {
  StaticJsonDocument<64> doc3;
  char json[64];

  doc3["temp"] = sensor_temp;
  doc3["humi"] = sensor_humi;
  doc3["co2"] = sensor_co2;

  serializeJson(doc3, json);

  Serial.print("Send a message with topic: ");
  Serial.println(topic_send_2);
  Serial.println(json);
  Serial.println();

  mqttClient.beginMessage(topic_send_2);
  mqttClient.print(json);
  mqttClient.endMessage();
}