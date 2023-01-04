#include <PubSubClient.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

// 핀 번호 지정
#define SOL1 8
#define SOL2 9

char ssid[] = SSID;  // SSID
char pass[] = PASSWORD; // Password

WiFiClient wifiClient;
PubSubClient client(wifiClient);

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

  setup_wifi();
  client.setServer(broker, port);
  client.setCallback(callback);
}

void loop() {
  // MQTT 서버 연결이 되지 않았을 때, 재연결 시도
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

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

// WiFi 연결
void setup_wifi() {
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(500);
  }
  Serial.println("You're connected to the network");
  Serial.println();
}

// MQTT 서버 연결
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting to connect to the MQTT broker: ");
    Serial.println(broker);
    // 랜덤 client ID 생성
    String clientId = "Client-";
    clientId += String(random(0xffff), HEX);
    // MQTT 서버와 연결 시도
    if (client.connect(clientId.c_str())) {
      Serial.println("You're connected to the MQTT broker!");
      Serial.print("Subscribe Topic: ");
      Serial.print(topic_receive);
      Serial.print(", ");
      Serial.println(topic_receive_2);
      Serial.println();
      client.subscribe(topic_receive);
      client.subscribe(topic_receive_2);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // 다시 시도하기 전 5초 대기
      delay(5000);
    }
  }
}

// // MQTT 메시지 수신
void callback(char* topic, byte* payload, unsigned int length) {
  String buffer;

  Serial.print("Message arrived '");
  Serial.print(topic);
  Serial.println("'");

  for (int i = 0; i < length; i++) {
    buffer = buffer + (char)payload[i];
  }
  Serial.println(buffer);
  Serial.println();
  set_control(buffer);
}

// 공기압 발생기 작동
void set_control(String message) {
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
    Serial.println("===> Emergency Stop");
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

  client.publish(topic_send, json);
}