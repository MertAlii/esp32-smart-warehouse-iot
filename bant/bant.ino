/*
 * Proje: Akıllı Depo Sistemi - BANT BİRİMİ (ÇİFT MOTOR)
 * Senaryo: Wi-Fi -> Kalibrasyon -> Otonom Motor Başlatma -> Kör Modlu Renk Okuma -> TEYİT SİSTEMİ
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "config.h"
#include "topics.h"
#include "crypto.h"

#define PIN_ENA 14
#define PIN_IN1 27
#define PIN_IN2 26
#define PIN_ENB 32
#define PIN_IN3 33
#define PIN_IN4 25

#define PIN_S0  18
#define PIN_S1  19
#define PIN_S2  21
#define PIN_S3  22
#define PIN_OUT 35

#define PIN_LED_R 15  
#define PIN_LED_G 2   
#define PIN_LED_B 4   

#define PWM_FREQ 1000
#define PWM_RES  8
#define MOTOR_HIZ 80 

WiFiClient espClient;
PubSubClient mqtt(espClient);
const char* client_id = "bant_01";

bool sifrelemeAktif = false; 
unsigned long lastHeartbeat = 0;
unsigned long lastColorRead = 0;
bool bantCalisiyor = false;      
bool yeniUrunBekleniyor = true;
bool boslukBekleniyor = false; 

long bosluk_Clear = 0;
int TOLERANS = 30; 

void motorKontrol(bool calistir);
void ledYak(int r, int g, int b);
void kalibrasyonYap();
void setupWiFi();
void reconnectMQTT();
void publishMessage(const char* topic, StaticJsonDocument<128>& doc, int qos = 1);
long renkFrekansOku(int s2_durum, int s3_durum);
String renkBelirle(long r, long g, long b);
void renkKontrolVeGonder();

void setup() {
  Serial.begin(115200);

  pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT); pinMode(PIN_IN4, OUTPUT);
  ledcAttach(PIN_ENA, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_ENB, PWM_FREQ, PWM_RES);

  pinMode(PIN_S0, OUTPUT); pinMode(PIN_S1, OUTPUT);
  pinMode(PIN_S2, OUTPUT); pinMode(PIN_S3, OUTPUT);
  pinMode(PIN_OUT, INPUT);
  
  pinMode(PIN_LED_R, OUTPUT); pinMode(PIN_LED_G, OUTPUT); pinMode(PIN_LED_B, OUTPUT);
  ledYak(0, 0, 0); 

  digitalWrite(PIN_S0, HIGH); digitalWrite(PIN_S1, LOW);

  Serial.println("\n[1] Sistem basladi. Ag baglantisi kuruluyor...");
  setupWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT); 
  mqtt.setCallback(mqttCallback);
  reconnectMQTT();

  kalibrasyonYap();

  Serial.println("[3] Kalibrasyon bitti. Motorlar donmeye basliyor...");
  bantCalisiyor = true;
  motorKontrol(true);

  StaticJsonDocument<128> doc;
  doc["durum"] = "online"; doc["mesaj"] = "Bant hazir ve donuyor."; doc["timestamp"] = millis();
  publishMessage("akillidepo/bant/heartbeat", doc, 0);
  lastHeartbeat = millis();
}

void loop() {
  if (!mqtt.connected()) reconnectMQTT();
  mqtt.loop();
  
  unsigned long currentMillis = millis();

  if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) { 
    StaticJsonDocument<128> doc;
    doc["durum"] = "online"; doc["timestamp"] = currentMillis;
    publishMessage("akillidepo/bant/heartbeat", doc, 0); 
    lastHeartbeat = currentMillis;
  }

  if (bantCalisiyor) {
    if (currentMillis - lastColorRead > 150) { 
      renkKontrolVeGonder();
      lastColorRead = currentMillis;
    }
  }
}

void motorKontrol(bool calistir) {
  if (calistir) {
    digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW); ledcWrite(PIN_ENA, MOTOR_HIZ);
    digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW); ledcWrite(PIN_ENB, MOTOR_HIZ);
  } else {
    digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW); ledcWrite(PIN_ENA, 0);
    digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, LOW); ledcWrite(PIN_ENB, 0);
  }
}

void ledYak(int r, int g, int b) {
  digitalWrite(PIN_LED_R, r); digitalWrite(PIN_LED_G, g); digitalWrite(PIN_LED_B, b);
}

void kalibrasyonYap() {
  Serial.println("[2] --- KALIBRASYON BASLADI ---");
  Serial.println("LUTFEN BANDIN ONUNU BOS BIRAKIN");
  long toplamClear = 0; int ornekSayisi = 20; 
  for(int i = 0; i < ornekSayisi; i++) {
    toplamClear += renkFrekansOku(HIGH, LOW);
    Serial.print("."); delay(250); 
  }
  bosluk_Clear = toplamClear / ornekSayisi;
  Serial.println("\n-> Bosluk Degeri: " + String(bosluk_Clear));
}

void setupWiFi() {
  delay(10); WiFi.begin(WIFI_SSID, WIFI_PASSWORD); 
  while (WiFi.status() != WL_CONNECTED) { delay(1000); Serial.print("."); }
  Serial.println("\nWiFi Baglandi.");
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    if (mqtt.connect(client_id)) { 
      mqtt.subscribe(TOPIC_BANT_KOMUT);
      mqtt.subscribe("akillidepo/ayar/sifreleme"); 
    } else { delay(RECONNECT_DELAY); }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String strTopic = String(topic);
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  if (strTopic == "akillidepo/ayar/sifreleme") {
    sifrelemeAktif = (msg == "1");
    return; 
  }

  msg = sifreCoz(msg); 
  StaticJsonDocument<256> doc; 
  if (deserializeJson(doc, msg)) return;
  
  if (strTopic == TOPIC_BANT_KOMUT) {
    String komut = doc["komut"];
    if (komut == "baslat") {
      bantCalisiyor = true; motorKontrol(true); boslukBekleniyor = true; 
      Serial.println("KULE EMRİ: Bant basladi. (Kör Mod)");
    } 
    else if (komut == "dur") {
      bantCalisiyor = false; motorKontrol(false);
      Serial.println("KULE EMRİ: Bant durduruldu.");
    }
    // YENİ ÖZELLİK: TEYİT KONTROLÜ
    else if (komut == "kontrol") {
      long clearDeger = renkFrekansOku(HIGH, LOW);
      bool suAnBosluk = (abs(clearDeger - bosluk_Clear) <= TOLERANS);
      
      StaticJsonDocument<128> reply;
      if (suAnBosluk) {
        reply["renk"] = "bos";
        Serial.println("KULE EMRİ: Kontrol -> Zemin BOS (Urun dusmus)");
      } else {
        long r = renkFrekansOku(LOW, LOW); long g = renkFrekansOku(HIGH, HIGH); long b = renkFrekansOku(LOW, HIGH);
        String tRenk = renkBelirle(r, g, b);
        reply["renk"] = (tRenk != "bilinmeyen") ? tRenk : "hata";
        Serial.println("KULE EMRİ: Kontrol -> URUN HALA BURADA!");
      }
      publishMessage(TOPIC_BANT_URUN, reply, 1);
    }
  }
}

void renkKontrolVeGonder() {
  long clearDeger = renkFrekansOku(HIGH, LOW);
  bool suAnBosluk = (abs(clearDeger - bosluk_Clear) <= TOLERANS);

  if (boslukBekleniyor) {
    if (suAnBosluk) {
      boslukBekleniyor = false; yeniUrunBekleniyor = true; ledYak(0, 0, 0); 
      Serial.println(">> Urun banttan gecti, yeni urun araniyor...");
    }
    return; 
  }

  if (!suAnBosluk && yeniUrunBekleniyor) {
    long r = renkFrekansOku(LOW, LOW); long g = renkFrekansOku(HIGH, HIGH); long b = renkFrekansOku(LOW, HIGH);
    String tespitEdilenRenk = renkBelirle(r, g, b);
    
    if (tespitEdilenRenk != "bilinmeyen") {
      Serial.println("\n>>> URUN ALGILANDI! Renk: " + tespitEdilenRenk);
      if (tespitEdilenRenk == "kirmizi") ledYak(1, 0, 0); else if (tespitEdilenRenk == "yesil") ledYak(0, 1, 0);
      else if (tespitEdilenRenk == "mavi") ledYak(0, 0, 1); else if (tespitEdilenRenk == "sari") ledYak(1, 1, 0);

      bantCalisiyor = false; motorKontrol(false); yeniUrunBekleniyor = false; 

      StaticJsonDocument<128> doc; doc["renk"] = tespitEdilenRenk;
      publishMessage(TOPIC_BANT_URUN, doc, 1);
      Serial.println("Motor durduruldu, Kule'nin emri bekleniyor...");
    }
  }
}

long renkFrekansOku(int s2_durum, int s3_durum) {
  digitalWrite(PIN_S2, s2_durum); digitalWrite(PIN_S3, s3_durum); delay(15);
  return pulseIn(PIN_OUT, LOW, 30000); 
}

String renkBelirle(long r, long g, long b) {
  if (r < b && g < b && abs(r - g) < 25) return "sari";
  if (r < g && r < b) return "kirmizi";
  if (g < r && g < b) return "yesil";
  if (b < r && b < g) return "mavi";
  return "bilinmeyen";
}

void publishMessage(const char* topic, StaticJsonDocument<128>& doc, int qos) {
  String jsonStr; serializeJson(doc, jsonStr);
  mqtt.publish(topic, sifrele(jsonStr).c_str(), qos); 
}