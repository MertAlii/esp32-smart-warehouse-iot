/*
 * Proje: Akıllı Depo Sistemi - ROBOT KOL BİRİMİ
 * Görev: Kule'den komut alıp banttaki ürünü itmek ve PC'den gelen şifreleme şalterini dinlemek.
 */

#include <ESP8266WiFi.h>
#include <Servo.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// AKILLI DEPO ORTAK KÜTÜPHANELERİ
#include "config.h"
#include "topics.h"
#include "crypto.h"

// Ağ ve MQTT İstemcisi
WiFiClient espClient;
PubSubClient mqtt(espClient);
const char* client_id = "kol_01"; 

// DONANIM PİNLERİ
#define PIN_BASE     D1
#define PIN_SHOULDER D2
#define PIN_ELBOW    D3

// SERVO NESNELERİ VE DURUM DEĞİŞKENLERİ
Servo servoTaban;
Servo servoOmuz;
Servo servoDirsek;

bool sifrelemeAktif = false; // Siber Şalter

// --- İTME/VURMA FONKSİYONU ---
void iteHareketiYurut() {
  int beklemeAcisi = 90; // Ürünlerin altından geçebileceği yüksek açı
  int vurmaAcisi = 30;   // Ürüne çarpıp devireceği alçak açı

  Serial.println("Hareket: VUR!");
  servoDirsek.write(vurmaAcisi); 
  
  delay(300); // Ürünün banttan düşmesi için fiziksel zaman tanı
  
  servoDirsek.write(beklemeAcisi);
  Serial.println("Hareket: GERI CEKILDI!");
}

// --- WI-FI VE MQTT KURULUM FONKSİYONLARI ---
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
      mqtt.subscribe(TOPIC_KOL_KOMUT);
      mqtt.subscribe("akillidepo/ayar/sifreleme"); // SİBER ŞALTER KANALINI DİNLE
    } else {
      Serial.print("Hata, rc=");
      Serial.print(mqtt.state());
      Serial.println(" 5 sn sonra tekrar...");
      delay(RECONNECT_DELAY);
    }
  }
}

// --- MQTT GERİ ÇAĞIRMA (EMİRLERİ DİNLEME) ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String strTopic = String(topic);
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  // 1. SİBER ŞALTER KONTROLÜ (PC'den gelen emirler şifresiz okunur)
  if (strTopic == "akillidepo/ayar/sifreleme") {
    sifrelemeAktif = (msg == "1");
    Serial.println(sifrelemeAktif ? ">> SIFRELEME ACIK <<" : ">> SIFRELEME KAPALI <<");
    return; // Şalter mesajıydı, alt kısımlara inip boşuna json arama
  }

  // 2. NORMAL KOMUTLARI ÇÖZ
  msg = sifreCoz(msg);

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, msg)) return; // Bozuk JSON ise atla

  // 3. KULEDEN VUR EMRİ GELDİYSE
  if (strTopic == TOPIC_KOL_KOMUT) {
    String komut = doc["komut"];
    if (komut == "ite") {
      Serial.println("\n>>> KULE EMRİ ALINDI: Urun Banttan Itiliyor <<<");
      
      iteHareketiYurut(); // Robot kol fiziki olarak vurur

      // Kule'ye işlemin bittiğini bildir
      StaticJsonDocument<128> replyDoc;
      replyDoc["durum"] = "ok";
      replyDoc["timestamp"] = millis();
      String replyStr;
      serializeJson(replyDoc, replyStr);
      
      // Artık #if DEBUG yok, kripto kütüphanesi kendisi karar veriyor!
      mqtt.publish(TOPIC_KOL_TAMAM, sifrele(replyStr).c_str(), 1);
      
      Serial.println(">>> KULEYE BILDIRILDI: islem_tamam <<<\n");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nAkilli Depo - Robot Kol Birimi Basliyor...");

  // Motor Başlatma ve Varsayılan (Bekleme) Pozisyonuna Çekme
  servoTaban.attach(PIN_BASE);
  servoOmuz.attach(PIN_SHOULDER);
  servoDirsek.attach(PIN_ELBOW);
  
  servoTaban.write(90);
  servoOmuz.write(90);
  servoDirsek.write(90); // 90 Derece = Bekleme Konumu

  setupWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
}

void loop() {
  if (!mqtt.connected()) reconnectMQTT();
  mqtt.loop(); 

  // Heartbeat (Nabız) - 5 Saniyede bir Kule'ye "Ben Buradayım" der
  static unsigned long lastHeartbeat = 0;
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
    StaticJsonDocument<128> doc;
    doc["durum"] = "online";
    doc["timestamp"] = currentMillis;
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    // Şifreleme aktifse şifreler, değilse düz yollar
    mqtt.publish("akillidepo/kol/heartbeat", sifrele(jsonStr).c_str(), 0);
    
    lastHeartbeat = currentMillis;
  }
}