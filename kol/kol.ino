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
const char* client_id = "kol_01"; // config.h gereksinimi

// DONANIM PİNLERİ
#define PIN_BASE     D1
#define PIN_SHOULDER D2
#define PIN_ELBOW    D3

// SERVO NESNELERİ
Servo servoTaban;
Servo servoOmuz;
Servo servoDirsek;

// --- İTME/VURMA FONKSİYONU ---
void iteHareketiYurut() {
  int beklemeAcisi = 90; // Robot kolun geride beklediği açı
  int vurmaAcisi = 30;   // Banttaki ürüne uzanıp vurduğu açı

  Serial.println("Hareket: VUR!");
  servoDirsek.write(vurmaAcisi); 
  
  delay(250); // Vurma işlemi için fiziksel zaman tanı
  
  servoDirsek.write(beklemeAcisi);
  Serial.println("Hareket: GERI CEKILDI!");
}

// --- MQTT GERİ ÇAĞIRMA (KULEDEN EMİR GELDİĞİNDE) ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  
  #if DEBUG_NO_ENCRYPTION == 0
    msg = sifreCoz(msg); // Mesajı çöz
  #endif

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, msg)) return; // Bozuk JSON ise atla

  String strTopic = String(topic);
  
  // Eğer Kule'den komut geldiyse
  if (strTopic == TOPIC_KOL_KOMUT) {
    String komut = doc["komut"];
    if (komut == "ite") {
      Serial.println("\n>>> KULE EMRİ ALINDI: Ürün Banttan İtiliyor <<<");
      
      iteHareketiYurut(); // Robot kol fiziki olarak ürüne vurur

      // Kule'ye işlemin bittiğini bildir ki bant tekrar çalışsın
      StaticJsonDocument<128> replyDoc;
      replyDoc["durum"] = "ok";
      replyDoc["timestamp"] = millis();
      String replyStr;
      serializeJson(replyDoc, replyStr);

      #if DEBUG_NO_ENCRYPTION == 1
        mqtt.publish(TOPIC_KOL_TAMAM, replyStr.c_str(), 1);
      #else
        mqtt.publish(TOPIC_KOL_TAMAM, sifrele(replyStr).c_str(), 1); 
      #endif
      
      Serial.println(">>> KULEYE BİLDİRİLDİ: islem_tamam <<<\n");
    }
  }
}

// --- MQTT YENİDEN BAĞLANMA ---
void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT Baglaniliyor...");
    if (mqtt.connect(client_id)) { 
      Serial.println("Baglandi.");
      mqtt.subscribe(TOPIC_KOL_KOMUT); // Kule'nin komut kanalını dinlemeye başla
    } else {
      Serial.print("Hata, rc=");
      Serial.print(mqtt.state());
      Serial.println(" 5 sn sonra tekrar denenecek.");
      delay(RECONNECT_DELAY);
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
  servoDirsek.write(90);

  // Wi-Fi Başlatma (config.h içindeki değerleri kullanır)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi Baglaniliyor: ");
  Serial.print(WIFI_SSID);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Baglandi!");

  // MQTT Kurulumu
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
}

void loop() {
  if (!mqtt.connected()) {
    reconnectMQTT(); // MQTT koptuğunda yeniden bağlan
  }
  mqtt.loop(); // Gelen MQTT komutlarını dinlemeye devam et

  // 5 Saniyede bir Kule'ye Heartbeat gönder (Kule'nin OLED ekranında ON yazması için)
  static unsigned long lastHeartbeat = 0;
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
    StaticJsonDocument<128> doc;
    doc["durum"] = "online";
    doc["timestamp"] = currentMillis;
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    #if DEBUG_NO_ENCRYPTION == 1
      mqtt.publish("akillidepo/kol/heartbeat", jsonStr.c_str(), 0);
    #else
      mqtt.publish("akillidepo/kol/heartbeat", sifrele(jsonStr).c_str(), 0);
    #endif
    
    lastHeartbeat = currentMillis;
  }
}