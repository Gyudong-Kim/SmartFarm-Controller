#include <PubSubClient.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "arduino_secrets.h"

SoftwareSerial rs485(2, 3); // RX, TX

// 핀 번호 지정
#define SOL1 7   // 양액 A
#define SOL2 8   // 양액 B
#define SOL3 9  // 재배기 1
#define SOL4 10  // 재배기 2
#define PUMP1 11 // 펌프 (원수탱크 → 혼합탱크)
#define PUMP2 12 // 펌프 (혼합탱크 → 재배기)
#define MIX 13   // 양액 혼합

char ssid[] = SSID;  // SSID
char pass[] = PASSWORD; // Password

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// MQTT 세팅
const char broker[] = MQTT_BROKER;
int port = MQTT_PORT;
const char topic_receive[] = "nutrient_control";
const char topic_receive_2[] = "emergency_stop";
const char topic_send[] = "nutrient_status";
const char topic_send_2[] = "house/nutrient_data";

// 양액 제어기 설정 변수
float set_ph = 0;
float set_ec = 0;
int set_amount = 0;
int set_cultivator = 0;
unsigned long set_time = 0;

// 양액 데이터 (from Sensor)
float sensor_ph = 0;
float sensor_ec = 0;

// 기타 변수
unsigned long time_previous_sensing = 0;
unsigned long time_previous_publish = 0;
unsigned long time_previous_water = 0;
unsigned long time_previous_nutrient = 0;
unsigned long time_previous_A = 0;
unsigned long time_previous_B = 0;

int C_M_001_ing = 0;
int SOL1_ing = 0;
int SOL2_ing = 0;
int SOL3_ing = 0;
int SOL4_ing = 0;
int PUMP1_ing = 0;
int PUMP2_ing = 0;

int start_count = 0;
int end_count = 0;
int flag_A = 0;
int flag_B = 0;

void setup() {
  Serial.begin(9600);     // for Serial Monitor

  pinMode(SOL1, OUTPUT);  // 양액 A
  pinMode(SOL2, OUTPUT);  // 양액 B
  pinMode(SOL3, OUTPUT);  // 재배기 1
  pinMode(SOL4, OUTPUT);  // 재배기 2
  pinMode(PUMP1, OUTPUT); // 펌프 (원수탱크 → 혼합탱크)
  pinMode(PUMP2, OUTPUT); // 펌프 (혼합탱크 → 재배기)
  pinMode(MIX, OUTPUT);   // 양액 혼합

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

  unsigned long time_current = millis(); // 현재 시간
  // 5초마다 양액 데이터 센싱
  if(time_current - time_previous_sensing >= 5000) {
    time_previous_sensing = time_current;
    read_pe300();
  }
  // 30초마다 양액 및 상태 데이터 Publish
  if(time_current - time_previous_publish >= 30000) {
    time_previous_publish = time_current;
    data_publish();
    status_publish();
  }

  if(C_M_001_ing == 1) { // 양액 제어기 작동 중일 때
    // 원수탱크 → 혼합탱크 작동 중지
    if(PUMP1_ing == 1) {
      if(time_current - time_previous_water >= set_time) {
        digitalWrite(PUMP1, LOW);
        digitalWrite(MIX, HIGH);

        PUMP1_ing = 0;
        end_count++;

        Serial.println("===> Water supply stop");
        Serial.println();
      }
    }
    // 양액 A 작동 중지
    if(SOL1_ing == 1) {
      if(sensor_ph <= set_ph) {
        digitalWrite(SOL1, LOW);

        SOL1_ing = 0;
        end_count++;

        Serial.println("===> Nutrient A supply stop");
        Serial.println();
      }
    }
    // 양액 B 작동 중지
    if(SOL2_ing == 1) {
      if(sensor_ec >= set_ec) {
        digitalWrite(SOL2, LOW);

        SOL2_ing = 0;
        end_count++;

        Serial.println("===> Nutrient B supply stop");
        Serial.println();
      }
    }
    // 재배기로 양액 공급
    if(start_count != 0 && end_count != 0) {
      if(start_count == end_count) {
        if(PUMP2_ing == 0) {
          digitalWrite(MIX, LOW);
          Serial.println("===> Nutrient mix complete");
          Serial.println();
          
          if(set_cultivator == 0) {
            digitalWrite(SOL3, HIGH);
            digitalWrite(SOL4, HIGH);
          } else if(set_cultivator == 1) {
            digitalWrite(SOL3, HIGH);
            digitalWrite(SOL4, LOW);
          } else {
            digitalWrite(SOL3, LOW);
            digitalWrite(SOL4, HIGH);
          }

          digitalWrite(PUMP2, HIGH);

          time_previous_nutrient = millis();
          PUMP2_ing = 1;

          Serial.println("===> Nutrient supply start");
          Serial.println();
        }     
        else {
          if(time_current - time_previous_nutrient >= set_time) {
            digitalWrite(PUMP2, LOW);
            digitalWrite(SOL3, LOW);
            digitalWrite(SOL4, LOW);

            C_M_001_ing = 0;
            PUMP2_ing = 0;
            start_count = 0;
            end_count = 0;

            status_publish();
            Serial.println("===> Nutrient supply stop");
            Serial.println();
          }
        }
      }
    }

    // 양액 A ON/OFF
    if(SOL1_ing == 1) {
      if(flag_A == 0) {
        if(time_current - time_previous_A >= 1000) {
          time_previous_A = time_current;
          digitalWrite(SOL1, LOW);
          Serial.println("===> Nutrient A OFF");
          Serial.println();
          flag_A = 1;
        }
      } else {
        if(time_current - time_previous_A >= 1000) {
          time_previous_A = time_current;
          digitalWrite(SOL1, HIGH);
          Serial.println("===> Nutrient A ON");
          Serial.println();
          flag_A = 0;
        }
      }
    }
    // 양액 B ON/OFF
    if(SOL2_ing == 1) {
      if(flag_B == 0) {
        if(time_current - time_previous_B >= 1000) {
          time_previous_B = time_current;
          digitalWrite(SOL2, LOW);
          Serial.println("===> Nutrient B OFF");
          Serial.println();
          flag_B = 1;
        }
      } else {
        if(time_current - time_previous_B >= 1000) {
          time_previous_B = time_current;
          digitalWrite(SOL2, HIGH);
          Serial.println("===> Nutrient B ON");
          Serial.println();
          flag_B = 0;
        }
      }
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

// 양액 제어기 작동 세팅
void set_control(String message) {
  StaticJsonDocument<128> doc;
  deserializeJson(doc, message); // 문자열을 JSON형식으로 변환

  if(doc["code"]=="C_M_001") { // OP_CODE check
    set_cultivator = doc["cultivator"];
    set_ph = doc["ph"];
    set_ec = doc["ec"];
    set_amount = doc["amount"];
    
    if(set_ph < 0 || set_ph > 14 || set_ec < 0 || set_ec > 5) { // 비정상적인 양액 설정 Filter
      digitalWrite(SOL1, LOW);
      digitalWrite(SOL2, LOW);
      digitalWrite(SOL3, LOW);
      digitalWrite(SOL4, LOW);
      digitalWrite(PUMP1, LOW);
      digitalWrite(PUMP2, LOW);
      digitalWrite(MIX, LOW);

      C_M_001_ing = 0;

      status_publish();
      Serial.println("===> Nutrient Controller Stop");
      Serial.println();
    } else {
      if(set_cultivator == 0) {
        SOL3_ing = 1;
        SOL4_ing = 1;
        start_count = 3;
      } else if(set_cultivator == 1) {
        SOL3_ing = 1;
        SOL4_ing = 0;
        start_count = 2;
      } else {
        SOL3_ing = 0;
        SOL4_ing = 1;
        start_count = 2;
      }
      digitalWrite(PUMP1, HIGH);

      set_time = set_amount * 20; // 분당 유량(3L)를 시간으로 환산 (밀리초)
      time_previous_water = millis();

      C_M_001_ing = 1; // 양액 제어기 작동
      SOL1_ing = 1;    // 양액 A 작동
      SOL2_ing = 1;    // 양액 B 작동
      PUMP1_ing = 1;   // 윈수탱크 → 혼합탱크 작동
      
      status_publish();      
      Serial.println("===> Nutrient Controller Start");
      Serial.println();
      Serial.println("Save setting values");
      Serial.println();
    }
  } else if(doc["code"]=="C_S_001") { // 비상정지
    digitalWrite(SOL1, LOW);
    digitalWrite(SOL2, LOW);
    digitalWrite(SOL3, LOW);
    digitalWrite(SOL4, LOW);
    digitalWrite(PUMP1, LOW);
    digitalWrite(PUMP2, LOW);
    digitalWrite(MIX, LOW);

    C_M_001_ing = 0;
    SOL1_ing = 0;
    SOL2_ing = 0;
    SOL3_ing = 0;
    SOL4_ing = 0;
    PUMP1_ing = 0;
    PUMP2_ing = 0;
    start_count = 0;
    end_count = 0;
    
    status_publish();
    Serial.println("===> Emergency stop");
    Serial.println();
  } else {
    Serial.println("Received the wrong message");
    Serial.println();
  }
}

// 양액 제어기 작동상태 Publish
void status_publish() {
  StaticJsonDocument<64> doc2;
  char json[64];

  doc2["nutrient"] = C_M_001_ing;

  serializeJson(doc2, json);

  Serial.print("Send a message with topic: ");
  Serial.println(topic_send);
  Serial.println(json);
  Serial.println();

  client.publish(topic_send, json);
}

// 양액 데이터 Publish
void data_publish() {
  StaticJsonDocument<64> doc3;
  char json[64];

  doc3["ph"] = sensor_ph;
  doc3["ec"] = sensor_ec;

  serializeJson(doc3, json);

  Serial.print("Send a message with topic: ");
  Serial.println(topic_send_2);
  Serial.println(json);
  Serial.println();

  client.publish(topic_send_2, json);
}