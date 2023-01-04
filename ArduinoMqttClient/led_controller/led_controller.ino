#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

// 핀 번호 지정
#define LED 8

char ssid[] = SSID;  // SSID
char pass[] = PASSWORD; // Password

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// MQTT 세팅
const char broker[] = MQTT_BROKER;
int port = MQTT_PORT;
const char topic_receive[] = "led_control";
const char topic_send[] = "led_status";

// 상태 데이터 변수
unsigned long time_previous_publish = 0;
int C_M_003_ing = 0;

void setup() {
  Serial.begin(9600); // for Serial Monitor
  pinMode(LED, OUTPUT);

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

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  // MQTT 연결
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

  // 30초마다 상태 데이터 전송
  unsigned long time_current = millis();
  if(time_current - time_previous_publish >= 30000) {
    time_previous_publish = time_current;
    publish();
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

// 조명 제어기 작동
void control(String message) {
  StaticJsonDocument<64> doc;

  deserializeJson(doc, message);

  if(doc["code"] == "C_M_003") {
    digitalWrite(LED, HIGH);
    C_M_003_ing = 1;

    publish();
    Serial.println("===> LED ON");
    Serial.println();
  } else if(doc["code"] == "C_M_004") {
    digitalWrite(LED, LOW);
    C_M_003_ing = 0;

    publish();
    Serial.println("===> LED OFF");
    Serial.println();    
  } else {
    Serial.println("Received the wrong message");
  }
}

// 조명 제어기 작동상태 Publish
void publish() {
  StaticJsonDocument<64> doc2;
  char json[32];

  doc2["led"] = C_M_003_ing;
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