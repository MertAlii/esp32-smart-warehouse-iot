/*
 * Proje: Akıllı Depo Sistemi - ARABA BİRİMİ (AGV)
 * Donanım: ESP8266 (NodeMCU), L298N Motor Sürücü, HC-SR04 Mesafe Sensörü
 * Görev: Kule'den gelen hedeflere ürün taşır, engel algıladığında durur ve raporlar.
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ORTAK KÜTÜPHANELER
#include "config.h"
#include "topics.h"
#include "crypto.h"

// MOTOR PİNLERİ (L298N - ESP8266 Pin Map)
#define PIN_ENA D1  // Sağ motor hızı
#define PIN_IN1 D2  // Sağ motor yön 1
#define PIN_IN2 D3  // Sağ motor yön 2
#define PIN_IN3 D4  // Sol motor yön 1
#define PIN_IN4 D5  // Sol motor yön 2
#define PIN_ENB D6  // Sol motor hızı

// SENSÖR PİNLERİ (HC-SR04)
#define PIN_TRIG D7
#define PIN_ECHO D8

WiFiClient espClient;
PubSubClient mqtt(espClient);
const char* client_id = "araba_01";

// DURUM DEĞİŞKENLERİ
int motorHizi = 800; // 0-1023 (ESP8266 PWM Aralığı)
bool engelVar = false;
unsigned long lastHeartbeat = 0;
unsigned long lastSensorRead = 0;

// FONKSİYON BİLDİRİMLERİ
void motorKontrol(String yon, int sureMs = 0);
void setupWiFi();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool engelKontrol();

void setup() {
  Serial.begin(115200);
  Serial.println("\nAkilli Depo - Araba Birimi Basliyor...");

  // Motor Pin Ayarları
  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  pinMode(PIN_ENB, OUTPUT);
  
  // Sensör Pin Ayarları
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  motorKontrol("dur");

  setupWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  
  Serial.println("Araba Hazir!");
}

void loop() {
  if (!mqtt.connected()) {
    reconnectMQTT();
  }
  mqtt.loop();

  unsigned long currentMillis = millis();

  // 1. ENGEL KONTROLÜ (200ms'de bir)
  if (currentMillis - lastSensorRead > 200) {
    bool yeniEngel = engelKontrol();
    if (yeniEngel != engelVar) {
      engelVar = yeniEngel;
      
      // Kuleye engel durumunu bildir
      StaticJsonDocument<128> doc;
      doc["tip"] = "engel";
      doc["engel"] = engelVar;
      doc["timestamp"] = currentMillis;
      
      String jsonStr;
      serializeJson(doc, jsonStr);
      
      #if DEBUG_NO_ENCRYPTION == 1
        mqtt.publish(TOPIC_ARABA_DURUM, jsonStr.c_str());
      #else
        mqtt.publish(TOPIC_ARABA_DURUM, sifrele(jsonStr).c_str());
      #endif

      if (engelVar) {
        motorKontrol("dur");
        Serial.println(">> ENGEL ALGILANDI! Duruluyor...");
      } else {
        Serial.println(">> Yol temizlendi.");
      }
    }
    lastSensorRead = currentMillis;
  }

  // 2. HEARTBEAT (5 saniyede bir)
  if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
    StaticJsonDocument<128> doc;
    doc["durum"] = "online";
    doc["timestamp"] = currentMillis;
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    #if DEBUG_NO_ENCRYPTION == 1
      mqtt.publish("akillidepo/araba/heartbeat", jsonStr.c_str(), 0);
    #else
      mqtt.publish("akillidepo/araba/heartbeat", sifrele(jsonStr).c_str(), 0);
    #endif
    
    lastHeartbeat = currentMillis;
  }
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi Baglaniliyor: ");
  Serial.print(WIFI_SSID);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Baglandi!");
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT Baglaniliyor...");
    if (mqtt.connect(client_id)) {
      Serial.println("Baglandi.");
      mqtt.subscribe(TOPIC_ARABA_KOMUT);
    } else {
      Serial.print("Hata, rc=");
      Serial.print(mqtt.state());
      Serial.println(" 5 sn sonra tekrar...");
      delay(RECONNECT_DELAY);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  #if DEBUG_NO_ENCRYPTION == 0
    msg = sifreCoz(msg);
  #endif

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, msg)) return;

  String strTopic = String(topic);
  if (strTopic == TOPIC_ARABA_KOMUT) {
    String komut = doc["komut"];
    String hedef = doc["hedef"];
    
    if (komut == "git") {
      if (engelVar) {
        Serial.println("HATA: Yol kapali, gidemem!");
        return;
      }
      
      Serial.println("\n>>> KULE EMRİ: " + hedef + " hedefine gidiliyor...");
      
      // Senaryo gereği hedefe gitme simülasyonu
      motorKontrol("ileri", 3000); 
      
      // Teslimat tamamlandı bildirimi
      StaticJsonDocument<128> reply;
      reply["tip"] = "teslim";
      reply["hedef"] = hedef;
      reply["durum"] = "ok";
      reply["timestamp"] = millis();
      
      String replyStr;
      serializeJson(reply, replyStr);
      
      #if DEBUG_NO_ENCRYPTION == 1
        mqtt.publish(TOPIC_ARABA_DURUM, replyStr.c_str(), 1);
      #else
        mqtt.publish(TOPIC_ARABA_DURUM, sifrele(replyStr).c_str(), 1);
      #endif
      
      Serial.println(">>> TESLIMAT TAMAMLANDI: " + hedef + "\n");
    }
    else if (komut == "dur") {
      motorKontrol("dur");
      Serial.println("KULE EMRİ: Araba durduruldu.");
    }
  }
}

void motorKontrol(String yon, int sureMs) {
  if (yon == "ileri") {
    analogWrite(PIN_ENA, motorHizi);
    digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
    analogWrite(PIN_ENB, motorHizi);
    digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
  } 
  else if (yon == "geri") {
    analogWrite(PIN_ENA, motorHizi);
    digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, HIGH);
    analogWrite(PIN_ENB, motorHizi);
    digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, HIGH);
  }
  else if (yon == "dur") {
    analogWrite(PIN_ENA, 0); digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW);
    analogWrite(PIN_ENB, 0); digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, LOW);
  }
  
  if (sureMs > 0) {
    delay(sureMs);
    motorKontrol("dur");
  }
}

bool engelKontrol() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long sure = pulseIn(PIN_ECHO, HIGH, 30000); // 30ms timeout
  int mesafe = sure * 0.034 / 2;
  
  // Mesafe 0 gelirse okuma hatasıdır (çok uzak veya çok yakın)
  if (mesafe == 0) return false;
  
  return (mesafe < 15); // 15cm altı engel kabul edilir
}