/*
 * Proje: Akıllı Depo Sistemi - BANT BİRİMİ (ÇİFT MOTOR)
 * Senaryo: Otonom Motor Başlatma -> Hareketli Kalibrasyon -> Wi-Fi/MQTT -> Online Bildirimi
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ORTAK KÜTÜPHANELER
#include "config.h"
#include "topics.h"
#include "crypto.h"

// DONANIM PİNLERİ - MOTOR A (OUT1-2)
#define PIN_ENA 14
#define PIN_IN1 27
#define PIN_IN2 26

// DONANIM PİNLERİ - MOTOR B (OUT3-4)
#define PIN_ENB 32
#define PIN_IN3 33
#define PIN_IN4 25

// SENSÖR PİNLERİ (TCS3200)
#define PIN_S0  18
#define PIN_S1  19
#define PIN_S2  21
#define PIN_S3  22
#define PIN_OUT 35

// PWM Ayarları (ESP32 Core v3 Uyumlu)
#define PWM_FREQ 1000
#define PWM_RES  8
#define MOTOR_HIZ 80 // 0-255 arası bant hızı

WiFiClient espClient;
PubSubClient mqtt(espClient);
const char* client_id = "bant_01";

// DURUM DEĞİŞKENLERİ
unsigned long lastHeartbeat = 0;
unsigned long lastColorRead = 0;
bool bantCalisiyor = false;      
bool yeniUrunBekleniyor = true;

// KALİBRASYON DEĞİŞKENLERİ
long bosluk_Clear = 0;
int TOLERANS = 30; // Boşluktan sapma toleransı

// FONKSİYON BİLDİRİMLERİ
void motorKontrol(bool calistir);
void kalibrasyonYap();
void setupWiFi();
void reconnectMQTT();
void publishMessage(const char* topic, StaticJsonDocument<128>& doc, int qos = 1);
long renkFrekansOku(int s2_durum, int s3_durum);
String renkBelirle(long r, long g, long b);
void renkKontrolVeGonder();

void setup() {
  Serial.begin(115200);

  // 1. PİN VE DONANIM KURULUMLARI
  pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT); pinMode(PIN_IN4, OUTPUT);
  ledcAttach(PIN_ENA, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_ENB, PWM_FREQ, PWM_RES);

  pinMode(PIN_S0, OUTPUT); pinMode(PIN_S1, OUTPUT);
  pinMode(PIN_S2, OUTPUT); pinMode(PIN_S3, OUTPUT);
  pinMode(PIN_OUT, INPUT);

  // Sensör Frekans Ölçeklendirme (%20)
  digitalWrite(PIN_S0, HIGH);
  digitalWrite(PIN_S1, LOW);

  // 2. SİSTEM AÇILIR AÇILMAZ MOTORLARI BAŞLAT (İSTENEN ÖZELLİK 1)
  Serial.println("\n[1] Sistem basladi. Motorlar donuyor...");
  bantCalisiyor = true;
  motorKontrol(true);

  // 3. MOTOR DÖNERKEN ARKA PLANDA 10 SANİYE KALİBRASYON (İSTENEN ÖZELLİK 2)
  kalibrasyonYap();

  // 4. AĞ VE MQTT BAĞLANTILARINI YAP
  Serial.println("[3] Kalibrasyon bitti. Ag baglantisi basliyor...");
  setupWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT); 
  mqtt.setCallback(mqttCallback);

  // İlk bağlantıyı zorla kur
  reconnectMQTT();

  // 5. "BEN ÇALIŞIYORUM" BİLDİRİMİNİ GÖNDER (İSTENEN ÖZELLİK 3)
  StaticJsonDocument<128> doc;
  doc["durum"] = "online";
  doc["mesaj"] = "Bant kalibre edildi ve donuyor.";
  doc["timestamp"] = millis();
  publishMessage("akillidepo/bant/heartbeat", doc, 0);
  lastHeartbeat = millis();
  
  Serial.println("[4] Sisteme baglanildi ve Online bildirimi gonderildi. Bant hazir!\n");
}

void loop() {
  if (!mqtt.connected()) {
    reconnectMQTT(); 
  }
  mqtt.loop();
  
  unsigned long currentMillis = millis();

  // Düzenli olarak sisteme "Ben buradayım ve çalışıyorum" (Heartbeat) gönder
  if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) { 
    StaticJsonDocument<128> doc;
    doc["durum"] = "online";
    doc["timestamp"] = currentMillis;
    publishMessage("akillidepo/bant/heartbeat", doc, 0); 
    lastHeartbeat = currentMillis;
  }

  // Bant çalışıyorsa sensörü sürekli dinle (150ms'de bir)
  if (bantCalisiyor) {
    if (currentMillis - lastColorRead > 150) { 
      renkKontrolVeGonder();
      lastColorRead = currentMillis;
    }
  }
}

// ---------------- FONKSİYONLAR ----------------

void motorKontrol(bool calistir) {
  if (calistir) {
    // Motor A (OUT1-2)
    digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
    ledcWrite(PIN_ENA, MOTOR_HIZ);
    // Motor B (OUT3-4)
    digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
    ledcWrite(PIN_ENB, MOTOR_HIZ);
  } else {
    // Motor A ve B Durdur
    digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW);
    ledcWrite(PIN_ENA, 0);
    digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, LOW);
    ledcWrite(PIN_ENB, 0);
  }
}

void kalibrasyonYap() {
  Serial.println("[2] --- 10 SANIYELIK DINAMIK KALIBRASYON BASLADI ---");
  Serial.println("LUTFEN BANDIN ONUNU BOS BIRAKIN (Bant donerken okuma yapiliyor)");
  
  long toplamClear = 0;
  int ornekSayisi = 40; // 40 örnek * 250ms = 10 Saniye
  
  for(int i = 0; i < ornekSayisi; i++) {
    toplamClear += renkFrekansOku(HIGH, LOW); // Sadece Clear kanalını oku
    Serial.print(".");
    delay(250); 
  }
  
  bosluk_Clear = toplamClear / ornekSayisi;
  Serial.println("\n-> Kalibrasyon Tamamlandi! Ogrenilen Bosluk Degeri: " + String(bosluk_Clear));
}

void setupWiFi() {
  delay(10);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); 
  int deneme = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    deneme++;
    if(deneme > 15) ESP.restart(); // 15 saniyede bağlanamazsa yeniden başla
  }
  Serial.println("\nWiFi Baglandi.");
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT Broker'a Baglaniliyor...");
    if (mqtt.connect(client_id)) { 
      Serial.println("Baglandi.");
      mqtt.subscribe(TOPIC_BANT_KOMUT);
    } else {
      Serial.println(" Basarisiz! 5 sn icinde tekrar denenecek.");
      delay(RECONNECT_DELAY); 
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  #if DEBUG_NO_ENCRYPTION == 0
    msg = sifreCoz(msg); 
  #endif

  StaticJsonDocument<256> doc; 
  if (deserializeJson(doc, msg)) return;

  String strTopic = String(topic);
  if (strTopic == TOPIC_BANT_KOMUT) {
    String komut = doc["komut"];
    if (komut == "baslat") {
      bantCalisiyor = true;
      motorKontrol(true);
      yeniUrunBekleniyor = true; 
      Serial.println("KULE EMRİ: Bant tekrar baslatildi.");
    } 
    else if (komut == "dur") {
      bantCalisiyor = false;
      motorKontrol(false);
      Serial.println("KULE EMRİ: Bant durduruldu.");
    }
  }
}

void renkKontrolVeGonder() {
  long clearDeger = renkFrekansOku(HIGH, LOW);
  
  // Eğer değer boşluk toleransı içindeyse önü BOŞTUR
  if (abs(clearDeger - bosluk_Clear) <= TOLERANS) {
    return; // Bir şey yapma, dönmeye devam et
  } 
  // Tolerans dışındaysa CİSİM VARDIR
  else {
    if (yeniUrunBekleniyor) {
      long r = renkFrekansOku(LOW, LOW);
      long g = renkFrekansOku(HIGH, HIGH);
      long b = renkFrekansOku(LOW, HIGH);

      String tespitEdilenRenk = renkBelirle(r, g, b);
      
      if (tespitEdilenRenk != "bilinmeyen") {
        Serial.println("\n>>> URUN ALGILANDI! Renk: " + tespitEdilenRenk);
        
        // ÜRÜN BULUNDUĞU GİBİ MOTORU ANINDA DURDUR!
        bantCalisiyor = false;
        motorKontrol(false);
        
        // RENGİ KULEYE GÖNDER
        StaticJsonDocument<128> doc;
        doc["renk"] = tespitEdilenRenk;
        publishMessage(TOPIC_BANT_URUN, doc, 1);
        
        // Sistemi kilitle, Kule'den "baslat" emri gelene kadar yeni ürün aramasını engelle
        yeniUrunBekleniyor = false;
        Serial.println("Motor aninda durduruldu, Kule'nin emri bekleniyor...");
      }
    }
  }
}

long renkFrekansOku(int s2_durum, int s3_durum) {
  digitalWrite(PIN_S2, s2_durum);
  digitalWrite(PIN_S3, s3_durum);
  delay(15);
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
  String jsonStr;
  serializeJson(doc, jsonStr);

  #if DEBUG_NO_ENCRYPTION == 1
    mqtt.publish(topic, jsonStr.c_str(), qos);
  #else
    mqtt.publish(topic, sifrele(jsonStr).c_str(), qos); 
  #endif
}