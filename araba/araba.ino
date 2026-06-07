/*
 * Proje: Akıllı Depo Sistemi - ARABA BİRİMİ (AGV)
 * Görev: Kule'den gelen emirle hedefe gider, 3 saniye bekler ve geri döner.
 * Özellik: Sensörsüz, Otonom Zamanlı Sürüş, Siber Şalter Dinleme ve Donanımsal Ters Motor Düzeltmesi
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ORTAK KÜTÜPHANELER
#include "config.h"
#include "topics.h"
#include "crypto.h"

// MOTOR PİNLERİ (L298N - ESP8266 Pin Map)
#define PIN_ENA D1  
#define PIN_IN1 D2  
#define PIN_IN2 D3  
#define PIN_IN3 D4  
#define PIN_IN4 D5  
#define PIN_ENB D6  

WiFiClient espClient;
PubSubClient mqtt(espClient);
const char* client_id = "araba_01";

// SİSTEM VE DURUM DEĞİŞKENLERİ
bool sifrelemeAktif = false; // Siber Şalter
int motorHizi = 800;         // 0-1023 (ESP8266 PWM Aralığı)
unsigned long lastHeartbeat = 0;

// FONKSİYON BİLDİRİMLERİ
void motorKontrol(String yon);
void sistemiBeklet(int sureMs);
void setupWiFi();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);

void setup() {
  Serial.begin(115200);
  Serial.println("\nAkilli Depo - Araba Birimi Basliyor...");

  // Motor Pin Ayarları
  pinMode(PIN_ENA, OUTPUT); pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT); pinMode(PIN_ENB, OUTPUT);
  motorKontrol("dur");

  setupWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  
  Serial.println("Araba Hazir!");
}

void loop() {
  if (!mqtt.connected()) reconnectMQTT();
  mqtt.loop();
  unsigned long currentMillis = millis();

  // HEARTBEAT (Nabız) - 5 saniyede bir Kule'ye "Online" bildirimi
  if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
    StaticJsonDocument<128> doc;
    doc["durum"] = "online";
    doc["timestamp"] = currentMillis;
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    mqtt.publish("akillidepo/araba/heartbeat", sifrele(jsonStr).c_str(), 0);
    lastHeartbeat = currentMillis;
  }
}

// ARABA YOLDAYKEN KULE KOPUK SANMASIN DİYE ÖZEL BEKLETME FONKSİYONU
void sistemiBeklet(int sureMs) {
  unsigned long baslangic = millis();
  while (millis() - baslangic < sureMs) {
    mqtt.loop();
    // Ağ trafiğini dinlemeye devam et
    unsigned long currentMillis = millis();
    // Yoldayken de Kule'ye "Ben Hayattayım" mesajı (Heartbeat) at!
    if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
      StaticJsonDocument<128> doc;
      doc["durum"] = "online"; 
      doc["timestamp"] = currentMillis;
      String jsonStr; serializeJson(doc, jsonStr);
      mqtt.publish("akillidepo/araba/heartbeat", sifrele(jsonStr).c_str(), 0);
      lastHeartbeat = currentMillis;
    }
    delay(50);
  }
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi Baglaniliyor: ");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Baglandi!");
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT Baglaniliyor...");
    if (mqtt.connect(client_id)) {
      Serial.println("Baglandi.");
      mqtt.subscribe(TOPIC_ARABA_KOMUT);
      mqtt.subscribe("akillidepo/ayar/sifreleme");
      // SİBER ŞALTER KANALI
    } else {
      Serial.println(" 5 sn sonra tekrar...");
      delay(RECONNECT_DELAY);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String strTopic = String(topic);
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  
  // 1. SİBER ŞALTER KONTROLÜ (Şifresiz)
  if (strTopic == "akillidepo/ayar/sifreleme") {
    sifrelemeAktif = (msg == "1");
    Serial.println(sifrelemeAktif ? ">> SIFRELEME ACIK <<" : ">> SIFRELEME KAPALI <<");
    return;
  }

  // 2. NORMAL KOMUTLARI ÇÖZ
  msg = sifreCoz(msg);

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, msg)) return;
  
  if (strTopic == TOPIC_ARABA_KOMUT) {
    String komut = doc["komut"];
    String hedef = doc["hedef"];
    if (komut == "git") {
      Serial.println("\n>>> KULE EMRİ: Teslimata gidiliyor...");
      // 1. DÜMDÜZ GİT
      motorKontrol("ileri");
      sistemiBeklet(4000);
      // 4 Saniye ileri (Kendi parkuruna göre bu süreyi değiştirebilirsin)
      
      // 2. HEDEFTE 3 SANİYE DUR
      motorKontrol("dur");
      Serial.println("Hedefte 3 Saniye Bekleniyor...");
      sistemiBeklet(3000);
      
      // 3. GERİ GERİ AYNI NOKTAYA DÖN
      Serial.println("Geri Donuluyor...");
      motorKontrol("geri");
      sistemiBeklet(4000); // 4 Saniye geri dön
      motorKontrol("dur");
      
      // BİTTİĞİNİ KULEYE BİLDİR
      StaticJsonDocument<128> reply;
      reply["tip"] = "teslim";
      reply["durum"] = "ok";
      String replyStr;
      serializeJson(reply, replyStr);
      mqtt.publish(TOPIC_ARABA_DURUM, sifrele(replyStr).c_str(), 1);
      
      Serial.println(">>> TESLIMAT TAMAMLANDI!\n");
    }
    else if (komut == "dur") {
      motorKontrol("dur");
      Serial.println("KULE EMRİ: Araba durduruldu.");
    }
  }
}

// TERS BAĞLANTIYA GÖRE OPTİMİZE EDİLMİŞ MOTOR KONTROLÜ
void motorKontrol(String yon) {
  if (yon == "ileri") {
    // Fiziksel bağlantın ters olduğu için ileri komutunda motorları eskinin "geri" sinyalleriyle sürüyoruz
    analogWrite(PIN_ENA, motorHizi); digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, HIGH);
    analogWrite(PIN_ENB, motorHizi); digitalWrite(PIN_IN3, LOW);  digitalWrite(PIN_IN4, HIGH);
  } 
  else if (yon == "geri") {
    // Geri komutunda motorları eskinin "ileri" sinyalleriyle sürüyoruz
    analogWrite(PIN_ENA, motorHizi); digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
    analogWrite(PIN_ENB, motorHizi); digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
  }
  else if (yon == "dur") {
    analogWrite(PIN_ENA, 0); digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW);
    analogWrite(PIN_ENB, 0); digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, LOW);
  }
}