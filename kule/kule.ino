/*
 * Proje: Akıllı Depo Sistemi - KULE BİRİMİ (Birim 1)
 * Donanım: ESP32 DevKit v1, SH1106 OLED (1.3 inç), 3x LED, Buzzer, HW-483 Buton
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ORTAK KÜTÜPHANELER
#include "config.h"
#include "topics.h"
#include "crypto.h"

// GÜNCEL DONANIM PİNLERİ
#define PIN_SDA         21
#define PIN_SCL         22
#define PIN_LED_KIRMIZI 4   
#define PIN_LED_SARI    5   
#define PIN_LED_YESIL   2   
#define PIN_BUZZER      18  
#define PIN_BUTON       15  // HW-483 Reset Butonu

// OLED EKRAN AYARLARI
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// AĞ VE MQTT İSTEMCİSİ
WiFiClient espClient;
PubSubClient mqtt(espClient);
const char* client_id = "kule_01";

// İLK AÇILIŞ (RETAINED MESAJ) KORUMASI
bool ilkBaglanti = true;
unsigned long baglantiZamani = 0;

// DURUM MAKİNESİ
enum KuleDurumu { BEKLEME, URUN_ALGILANDI, KOL_BEKLENIYOR, ARABA_BEKLENIYOR, HATA };
KuleDurumu mevcutDurum = BEKLEME;
unsigned long stateTimer = 0;

// SİSTEM DEĞİŞKENLERİ
unsigned long lastDisplayUpdate = 0;
int currentPage = 0; 

// CİHAZ DURUMLARI VE HEARTBEAT
unsigned long hbBant = 0;
unsigned long hbKol = 0;
unsigned long hbAraba = 0;
const unsigned long HB_TIMEOUT = 8000; // 8 saniye

// BİLGİ TUTUCULAR
String sonUrunRenk = "-";
String sonUrunHedef = "-";
bool sonUrunItildi = false;
String bantDurum = "BEKLIYOR";
int islenenUrunSayisi = 0;
String arabaSonKomut = "-";
String arabaSonHedef = "-";
bool arabaEngelVar = false;

// LOG YÖNETİMİ
String sysLogs[3] = {"Sistem Basladi", "", ""};

// FONKSİYON BİLDİRİMLERİ
void setupWiFi();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void kararVer(String renk);
void durumKontrol();
void ledVeBuzzerGuncelle();
void ekranGuncelle();
void addLog(String msj);
void sendBantKomut(String komut);
void sendKolKomut(String komut, String hedefKutu);
void sendArabaKomut(String komut, String hedef);

void setup() {
  Serial.begin(115200);

  // Pin Ayarları
  pinMode(PIN_LED_KIRMIZI, OUTPUT);
  pinMode(PIN_LED_SARI, OUTPUT);
  pinMode(PIN_LED_YESIL, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  // HW-483 için INPUT_PULLUP kullanıyoruz. Basılmadığında zorla HIGH kalacak.
  pinMode(PIN_BUTON, INPUT_PULLUP); 
  
  // Açılış Animasyonu
  digitalWrite(PIN_LED_YESIL, HIGH);
  delay(500);
  digitalWrite(PIN_LED_YESIL, LOW);

  // OLED Başlatma
  Wire.begin(PIN_SDA, PIN_SCL);
  if(!display.begin(0x3C, true)) { 
    Serial.println("SH1106 OLED basarisiz!");
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 20);
  display.println("KULE BASLIYOR...");
  display.display();

  setupWiFi();
  
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  addLog("WiFi & MQTT OK");
}

void loop() {
  if (!mqtt.connected()) {
    reconnectMQTT();
  }
  mqtt.loop();

  unsigned long currentMillis = millis();

  // ---------------------------------------------------------
  // MANUEL RESET BUTONU KONTROLÜ (Active-LOW + Görsel Şov)
  // ---------------------------------------------------------
  if (digitalRead(PIN_BUTON) == LOW) { // Butona basıldığında LOW olur
    if (mevcutDurum == HATA) {
      mevcutDurum = BEKLEME;
      addLog("Manuel Sifirlama!");
      
      // Buzzer'ı anında sustur
      digitalWrite(PIN_BUZZER, LOW);
      
      // Kurtarma Animasyonu: Tüm LED'ler 3 kez yanıp sönsün
      for(int i = 0; i < 3; i++) {
        digitalWrite(PIN_LED_KIRMIZI, HIGH);
        digitalWrite(PIN_LED_SARI, HIGH);
        digitalWrite(PIN_LED_YESIL, HIGH);
        delay(200);
        digitalWrite(PIN_LED_KIRMIZI, LOW);
        digitalWrite(PIN_LED_SARI, LOW);
        digitalWrite(PIN_LED_YESIL, LOW);
        delay(200);
      }

      // Her şey sıfırlandı, bantı tekrar döndür
      sendBantKomut("baslat"); 
    }
  }

  durumKontrol();
  ledVeBuzzerGuncelle();

  // Ekran geçiş süresi 3 saniye, toplam 6 sayfa
  if (currentMillis - lastDisplayUpdate > 3000) {
    currentPage = (currentPage + 1) % 6; 
    lastDisplayUpdate = currentMillis;
  }
  ekranGuncelle();

  // Kule Heartbeat
  static unsigned long lastHeartbeat = 0;
  if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
    StaticJsonDocument<128> doc;
    doc["durum"] = "online";
    doc["timestamp"] = currentMillis;
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    #if DEBUG_NO_ENCRYPTION == 1
      mqtt.publish("akillidepo/kule/heartbeat", jsonStr.c_str(), 0);
    #else
      mqtt.publish("akillidepo/kule/heartbeat", sifrele(jsonStr).c_str(), 0);
    #endif
    lastHeartbeat = currentMillis;
  }
}

// ---------------- WI-FI VE MQTT ----------------

void setupWiFi() {
  delay(10);
  Serial.print("WiFi Baglaniyor: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int deneme = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    deneme++;
    if(deneme > 15) ESP.restart(); 
  }
  Serial.println("\nWiFi Baglandi.");
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT Baglaniliyor...");
    if (mqtt.connect(client_id)) {
      Serial.println("Baglandi.");
      
      if (ilkBaglanti) {
        baglantiZamani = millis();
        ilkBaglanti = false;
      }

      mqtt.subscribe(TOPIC_BANT_URUN);
      mqtt.subscribe(TOPIC_KOL_TAMAM);
      mqtt.subscribe(TOPIC_ARABA_DURUM);
      mqtt.subscribe("akillidepo/+/heartbeat");
    } else {
      Serial.print("Hata, rc=");
      Serial.print(mqtt.state());
      Serial.println(" 5 sn sonra tekrar.");
      delay(RECONNECT_DELAY);
    }
  }
}

// ---------------- MQTT MESAJ İŞLEME ----------------

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (millis() - baglantiZamani < 2000) {
    return;
  }

  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  #if DEBUG_NO_ENCRYPTION == 0
    msg = sifreCoz(msg); 
  #endif

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) return;

  String strTopic = String(topic);

  if (strTopic.endsWith("/heartbeat")) {
    unsigned long nowMs = millis();
    if (strTopic.indexOf("bant") != -1) hbBant = nowMs;
    else if (strTopic.indexOf("kol") != -1) hbKol = nowMs;
    else if (strTopic.indexOf("araba") != -1) hbAraba = nowMs;
    return;
  }

  if (strTopic == TOPIC_BANT_URUN && mevcutDurum == BEKLEME) {
    String renk = doc["renk"];
    sonUrunRenk = renk;
    islenenUrunSayisi++;
    addLog("Urun: " + renk);
    mevcutDurum = URUN_ALGILANDI;
    kararVer(renk);
  }
  else if (strTopic == TOPIC_KOL_TAMAM && mevcutDurum == KOL_BEKLENIYOR) {
    addLog("Kol islemi tmm");
    mevcutDurum = BEKLEME;
    sonUrunItildi = true;
    sendBantKomut("baslat"); 
  }
  else if (strTopic == TOPIC_ARABA_DURUM) {
    String tip = doc["tip"];
    if (tip == "engel") {
      arabaEngelVar = doc["engel"];
      if(arabaEngelVar) addLog("Araba: ENGEL!");
      else addLog("Araba: Engel bitti");
    } 
    else if (tip == "teslim" && mevcutDurum == ARABA_BEKLENIYOR) {
      addLog("Araba: Teslim tmm");
      mevcutDurum = BEKLEME;
    }
  }
}

// ---------------- KARAR VE SÜREÇ MANTIĞI ----------------

void kararVer(String renk) {
  unsigned long nowMs = millis();
  bool kolOnline = (nowMs - hbKol < HB_TIMEOUT);
  bool arabaOnline = (nowMs - hbAraba < HB_TIMEOUT);

  if (renk == "kirmizi" || renk == "sari") {
    if (!kolOnline) {
      addLog("HATA: Kol Offline!");
      mevcutDurum = HATA;
      return;
    }
    sonUrunHedef = (renk == "kirmizi") ? "raf_A1" : "yan_kutu";
    mevcutDurum = KOL_BEKLENIYOR;
    stateTimer = millis();
    sendBantKomut("dur"); 
    sendKolKomut("ite", sonUrunHedef);
  } 
  else if (renk == "mavi" || renk == "yesil") {
    if (!arabaOnline) {
      addLog("HATA: Araba Offline!");
      mevcutDurum = HATA;
      return; 
    }
    sonUrunHedef = (renk == "mavi") ? "raf_B1" : "raf_A2";
    sonUrunItildi = false;
    mevcutDurum = ARABA_BEKLENIYOR;
    stateTimer = millis();
    sendBantKomut("baslat"); 
    sendArabaKomut("git", sonUrunHedef);
  } 
  else {
    addLog("Bilinmeyen renk");
    mevcutDurum = BEKLEME;
    sendBantKomut("baslat"); 
  }
}

void durumKontrol() {
  unsigned long nowMs = millis();
  
  if (mevcutDurum == KOL_BEKLENIYOR && (nowMs - stateTimer > KOL_TIMEOUT_MS)) {
    mevcutDurum = HATA;
    stateTimer = nowMs;
    addLog("HATA: Kol Timeout");
  }
  if (mevcutDurum == ARABA_BEKLENIYOR && !arabaEngelVar && (nowMs - stateTimer > ARABA_TIMEOUT_MS)) {
    mevcutDurum = HATA;
    stateTimer = nowMs;
    addLog("HATA: Araba Timeout");
  }
}

// ---------------- DONANIM VE GÖRSELLEŞTİRME ----------------

void ledVeBuzzerGuncelle() {
  unsigned long nowMs = millis();
  bool bantOnline = (nowMs - hbBant < HB_TIMEOUT);
  bool kolOnline = (nowMs - hbKol < HB_TIMEOUT);
  bool arabaOnline = (nowMs - hbAraba < HB_TIMEOUT);

  int bagliCihazSayisi = 0;
  if (bantOnline) bagliCihazSayisi++;
  if (kolOnline) bagliCihazSayisi++;
  if (arabaOnline) bagliCihazSayisi++;

  // Blink şovunun olduğu kurtarma anında bu fonksiyonun ışıkları ezmemesi için ufak bir koruma:
  // (mevcutDurum BEKLEME'ye döndüğü an normal statü renkleri yanar)
  
  digitalWrite(PIN_LED_YESIL, LOW);
  digitalWrite(PIN_LED_SARI, LOW);
  digitalWrite(PIN_LED_KIRMIZI, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  if (mevcutDurum == HATA) {
    if ((nowMs / 250) % 2 == 0) { 
      digitalWrite(PIN_LED_KIRMIZI, HIGH);
      digitalWrite(PIN_BUZZER, HIGH);
    }
  } 
  else if (bagliCihazSayisi == 0) {
    digitalWrite(PIN_LED_KIRMIZI, HIGH);
  } 
  else if (bagliCihazSayisi > 0 && bagliCihazSayisi < 3) {
    digitalWrite(PIN_LED_SARI, HIGH);
  } 
  else if (bagliCihazSayisi == 3) {
    digitalWrite(PIN_LED_YESIL, HIGH);
  }
}

void ekranGuncelle() {
  display.clearDisplay();
  display.setCursor(0, 0);
  
  unsigned long nowMs = millis();
  String sBant = (nowMs - hbBant < HB_TIMEOUT) ? "ON" : "OFF";
  String sKol = (nowMs - hbKol < HB_TIMEOUT) ? "ON" : "OFF";
  String sAraba = (nowMs - hbAraba < HB_TIMEOUT) ? "ON" : "OFF";

  switch (currentPage) {
    case 0:
      display.println("1/6: GENEL DURUM");
      display.drawLine(0, 10, 128, 10, SH110X_WHITE);
      display.setCursor(0, 15);
      display.println("Bant:  " + sBant);
      display.println("Kol:   " + sKol);
      display.println("Araba: " + sAraba);
      display.print("Sistem: ");
      if(mevcutDurum == BEKLEME) display.println("BEKLEME");
      else if(mevcutDurum == HATA) display.println("HATA!");
      else display.println("MESGUL");
      break;

    case 1: 
      display.println("2/6: SON URUN");
      display.drawLine(0, 10, 128, 10, SH110X_WHITE);
      display.setCursor(0, 15);
      display.println("Renk : " + sonUrunRenk);
      display.println("Hedef: " + sonUrunHedef);
      display.print("Durum: ");
      display.println(sonUrunItildi ? "Itildi" : "Arabada");
      break;

    case 2: 
      display.println("3/6: BANT DURUMU");
      display.drawLine(0, 10, 128, 10, SH110X_WHITE);
      display.setCursor(0, 15);
      display.println("Motor: " + bantDurum);
      display.print("Sayac: ");
      display.println(islenenUrunSayisi);
      break;

    case 3: 
      display.println("4/6: ARABA DURUMU");
      display.drawLine(0, 10, 128, 10, SH110X_WHITE);
      display.setCursor(0, 15);
      display.println("Komut: " + arabaSonKomut);
      display.println("Hedef: " + arabaSonHedef);
      display.print("Engel: ");
      display.println(arabaEngelVar ? "VAR!" : "Yok");
      break;

    case 4: 
      display.println("5/6: SISTEM LOG");
      display.drawLine(0, 10, 128, 10, SH110X_WHITE);
      display.setCursor(0, 15);
      display.println("> " + sysLogs[0]);
      display.println("> " + sysLogs[1]);
      display.println("> " + sysLogs[2]);
      break;

    case 5:
      display.println("6/6: GELISTIRICILER");
      display.drawLine(0, 10, 128, 10, SH110X_WHITE);
      display.setCursor(0, 15);
      display.println("- Mert Ali Alkan");
      display.println("- Umut Turker");
      display.println("- Berk Talha Aslan");
      break;
  }
  display.display();
}

void addLog(String msj) {
  sysLogs[2] = sysLogs[1];
  sysLogs[1] = sysLogs[0];
  sysLogs[0] = msj;
  Serial.println("LOG: " + msj);
}

// ---------------- YARDIMCI GÖNDERİM FONKSİYONLARI ----------------

void publishMessage(const char* topic, StaticJsonDocument<256>& doc, int qos = 1) {
  String jsonStr;
  serializeJson(doc, jsonStr);
  
  #if DEBUG_NO_ENCRYPTION == 1
    mqtt.publish(topic, jsonStr.c_str(), qos);
  #else
    mqtt.publish(topic, sifrele(jsonStr).c_str(), qos);
  #endif
}

void sendBantKomut(String komut) {
  StaticJsonDocument<256> doc;
  doc["komut"] = komut;
  publishMessage(TOPIC_BANT_KOMUT, doc);
  bantDurum = (komut == "baslat") ? "CALISIYOR" : "DURDU";
}

void sendKolKomut(String komut, String hedefKutu) {
  StaticJsonDocument<256> doc;
  doc["komut"] = komut;
  doc["hedef_kutu"] = hedefKutu;
  doc["timestamp"] = millis();
  publishMessage(TOPIC_KOL_KOMUT, doc);
}

void sendArabaKomut(String komut, String hedef) {
  StaticJsonDocument<256> doc;
  doc["komut"] = komut;
  doc["hedef"] = hedef;
  doc["adim_ms"] = 2000; 
  doc["donus_derece"] = 0;
  doc["adim2_ms"] = 0;
  
  arabaSonKomut = komut;
  arabaSonHedef = hedef;
  publishMessage(TOPIC_ARABA_KOMUT, doc);
}