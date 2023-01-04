#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

// 핀 번호 지정
#define SOL1 8
#define SOL2 9

char ssid[] = SSID;  // SSID
char pass[] = PASSWORD; // Password

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// MQTT 세팅
const char broker[] = MQTT_BROKER;
int port = MQTT_PORT;
const char topic_receive[] = "air_control";
const char topic_receive_2[] = "emergency_stop";
const char topic_send[] = "air_status";

// 공기압 발생기 제어 및 상태 데이터 변수
unsigned long time_previous = 0;
unsigned long time_previous_publish = 0;
unsigned long time = 0;
int C_M_002_ing = 0;

void setup() {
  Serial.begin(9600);    // for Serial Monitor
  pinMode(SOL1, OUTPUT); // 솔레노이드 1
  pinMode(SOL2, OUTPUT); // 솔레노이드 2

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

  // MQTT 메시지 수신 콜백 설정
  mqttClient.onMessage(onMqttMessage);

  // Subscribe 토픽
  Serial.print("Subscribing to topic: ");
  Serial.print(topic_receive);
  Serial.print(", ");
  Serial.println(topic_receive_2);

  mqttClient.subscribe(topic_receive);
  mqttClient.subscribe(topic_receive_2);

  Serial.println();
}

void loop() {
  // 연결이 끊기지 않도록 MQTT broker keep alive 전송
  mqttClient.poll();

  // 30초마다 상태 데이터 전송
  unsigned long time_current = millis();
  if(time_current - time_previous_publish >= 30000) {
    time_previous_publish = time_current;
    publish();
  }

  // 설정 시간(분 단위) 이후 공기압 발생기 작동 정지
  if(C_M_002_ing == 1) {
    if(time_current - time_previous >= time * 1000) {
      digitalWrite(SOL1, LOW);
      digitalWrite(SOL2, LOW);
      C_M_002_ing = 0;

      publish();
      Serial.println("===> Air OFF");
      Serial.println();
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
  control(buffer);
}

// 공기압 발생기 작동
void control(String message) {
  StaticJsonDocument<64> doc;
  deserializeJson(doc, message); // 문자열을 JSON형식으로 변환

  if(doc["code"] == "C_M_002") { // OP_CODE check
    if(doc["cultivator"] == 0) { // 재배기 전체 
      digitalWrite(SOL1, HIGH);
      digitalWrite(SOL2, HIGH);
      time = doc["time"];
      time_previous = millis();
      C_M_002_ing = 1;

      publish();
      Serial.println("===> Air ON (Cultivator All)");
      Serial.println();
    } else if(doc["cultivator"] == 1) { // 재배기 1번
      digitalWrite(SOL1, HIGH);
      time = doc["time"];
      time_previous = millis();
      C_M_002_ing = 1;

      publish();
      Serial.println("===> Air ON (Cultivator 1)");
      Serial.println();
    } else if(doc["cultivator"] == 2) { // 재배기 2번
      digitalWrite(SOL2, HIGH);
      time = doc["time"];
      time_previous = millis();
      C_M_002_ing = 1;

      publish();
      Serial.println("===> Air ON (Cultivator 1)");
      Serial.println();
    } else {
      Serial.println("Received the wrong message");
    }
  } else if(doc["code"] == "C_S_001") { // 비상정지
    digitalWrite(SOL1, LOW);
    digitalWrite(SOL2, LOW);
    C_M_002_ing = 0;
    
    publish();
    Serial.println("===> Emergency ");
    Serial.println();
  } else {
    Serial.println("Received the wrong message");
  }
}

// 공기압 발생기 작동상태 Publish
void publish() {
  StaticJsonDocument<64> doc2;
  char json[32];

  doc2["air"] = C_M_002_ing;
  serializeJson(doc2, json);

  Serial.print("Send a message with topic '");
  Serial.print(topic_send);
  Serial.println("'");
  Serial.println(json);
  Serial.println();

  mqttClient.beginMessage(topic_send);
  mqttClient.print(json);
  mqttClient.endMessage();
}