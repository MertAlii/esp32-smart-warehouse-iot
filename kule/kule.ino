/*
 * Proje: Akıllı Depo Sistemi - KULE BİRİMİ (Birim 1)
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#include "config.h"
#include "topics.h"
#include "crypto.h"

#define PIN_SDA         21
#define PIN_SCL         22
#define PIN_LED_KIRMIZI 4   
#define PIN_LED_SARI    5   
#define PIN_LED_YESIL   2   
#define PIN_BUZZER      18  
#define PIN_BUTON       15  

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WiFiClient espClient;
PubSubClient mqtt(espClient);
const char* client_id = "kule_01";

bool ilkBaglanti = true;
unsigned long baglantiZamani = 0;

// YENİ DURUM: ARABA_YUKLENIYOR eklendi
enum KuleDurumu { BEKLEME, URUN_ALGILANDI, KOL_BEKLENIYOR, TEYIT_BEKLENIYOR, ARABA_YUKLENIYOR, ARABA_BEKLENIYOR, HATA };
KuleDurumu mevcutDurum = BEKLEME;
unsigned long stateTimer = 0;

bool sifrelemeAktif = false; 
int arabaUrunSayaci = 0;     
unsigned long lastDisplayUpdate = 0;
int currentPage = 0; 

unsigned long hbBant = 0; unsigned long hbKol = 0; unsigned long hbAraba = 0;
const unsigned long HB_TIMEOUT = 8000;

String sonUrunRenk = "-"; String sonUrunHedef = "-"; bool sonUrunItildi = false;
String bantDurum = "BEKLIYOR"; int islenenUrunSayisi = 0;
String arabaSonKomut = "-"; String arabaSonHedef = "-";
String sysLogs[3] = {"Sistem Basladi", "", ""};

void setupWiFi(); void reconnectMQTT(); void mqttCallback(char* topic, byte* payload, unsigned int length);
void kararVer(String renk); void durumKontrol(); void ledVeBuzzerGuncelle();
void ekranGuncelle(); void addLog(String msj);
void sendBantKomut(String komut); void sendKolKomut(String komut, String hedefKutu);
void sendArabaKomut(String komut, String hedef);

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED_KIRMIZI, OUTPUT); pinMode(PIN_LED_SARI, OUTPUT); pinMode(PIN_LED_YESIL, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT); pinMode(PIN_BUTON, INPUT_PULLUP);
  digitalWrite(PIN_LED_YESIL, HIGH); delay(500); digitalWrite(PIN_LED_YESIL, LOW);

  Wire.begin(PIN_SDA, PIN_SCL);
  if(!display.begin(0x3C, true)) { Serial.println("SH1106 OLED basarisiz!"); for(;;); }
  
  display.clearDisplay(); display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 20); display.println("KULE BASLIYOR..."); display.display();

  setupWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  addLog("WiFi & MQTT OK");
}

void loop() {
  if (!mqtt.connected()) reconnectMQTT();
  mqtt.loop();

  unsigned long currentMillis = millis();
  if (digitalRead(PIN_BUTON) == LOW) {
    if (mevcutDurum == HATA) {
      mevcutDurum = BEKLEME;
      addLog("Manuel Sifirlama!"); digitalWrite(PIN_BUZZER, LOW);
      for(int i = 0; i < 3; i++) {
        digitalWrite(PIN_LED_KIRMIZI, HIGH);
        digitalWrite(PIN_LED_SARI, HIGH); digitalWrite(PIN_LED_YESIL, HIGH); delay(200);
        digitalWrite(PIN_LED_KIRMIZI, LOW); digitalWrite(PIN_LED_SARI, LOW); digitalWrite(PIN_LED_YESIL, LOW); delay(200);
      }
      sendBantKomut("baslat");
    }
  }

  durumKontrol(); ledVeBuzzerGuncelle();

  if (currentMillis - lastDisplayUpdate > 3000) { currentPage = (currentPage + 1) % 6;
  lastDisplayUpdate = currentMillis; }
  ekranGuncelle();

  static unsigned long lastHeartbeat = 0;
  if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
    StaticJsonDocument<128> doc; doc["durum"] = "online"; doc["timestamp"] = currentMillis;
    String jsonStr;
    serializeJson(doc, jsonStr);
    mqtt.publish("akillidepo/kule/heartbeat", sifrele(jsonStr).c_str(), 0);
    lastHeartbeat = currentMillis;
  }
}

void setupWiFi() {
  delay(10); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int deneme = 0;
  while (WiFi.status() != WL_CONNECTED) { delay(1000); Serial.print("."); deneme++; if(deneme > 15) ESP.restart(); }
  Serial.println("\nWiFi Baglandi.");
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    if (mqtt.connect(client_id)) {
      if (ilkBaglanti) { baglantiZamani = millis();
      ilkBaglanti = false; }
      mqtt.subscribe(TOPIC_BANT_URUN); mqtt.subscribe(TOPIC_KOL_TAMAM); mqtt.subscribe(TOPIC_ARABA_DURUM);
      mqtt.subscribe("akillidepo/+/heartbeat"); mqtt.subscribe("akillidepo/ayar/sifreleme"); 
    } else { delay(RECONNECT_DELAY);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (millis() - baglantiZamani < 2000) return;
  String msg = ""; for (int i = 0; i < length; i++) msg += (char)payload[i];
  String strTopic = String(topic);
  
  if (strTopic == "akillidepo/ayar/sifreleme") {
    sifrelemeAktif = (msg == "1"); addLog(sifrelemeAktif ? "Sifreleme: ACIK" : "Sifreleme: KAPALI");
    return; 
  }

  msg = sifreCoz(msg);
  StaticJsonDocument<256> doc; if (deserializeJson(doc, msg)) return;
  
  if (strTopic.endsWith("/heartbeat")) {
    unsigned long nowMs = millis();
    if (strTopic.indexOf("bant") != -1) hbBant = nowMs;
    else if (strTopic.indexOf("kol") != -1) hbKol = nowMs;
    else if (strTopic.indexOf("araba") != -1) hbAraba = nowMs; return;
  }

  if (strTopic == TOPIC_BANT_URUN) {
    String renk = doc["renk"];
    if (mevcutDurum == BEKLEME) {
      sonUrunRenk = renk;
      islenenUrunSayisi++; addLog("Urun: " + renk);
      mevcutDurum = URUN_ALGILANDI; kararVer(renk);
    }
    else if (mevcutDurum == TEYIT_BEKLENIYOR) {
      if (renk == "bos") {
        addLog("Teyit: Urun Dustu!");
        mevcutDurum = BEKLEME; sonUrunItildi = true; sendBantKomut("baslat"); 
      } else {
        addLog("Teyit: DUSMEDI! Tekrar!");
        mevcutDurum = KOL_BEKLENIYOR; stateTimer = millis(); sendKolKomut("ite", sonUrunHedef);
      }
    }
  }
  else if (strTopic == TOPIC_KOL_TAMAM && mevcutDurum == KOL_BEKLENIYOR) {
    addLog("Kol Vurdu, Teyit Istendi");
    mevcutDurum = TEYIT_BEKLENIYOR;
    stateTimer = millis();
    sendBantKomut("kontrol"); 
  }
  else if (strTopic == TOPIC_ARABA_DURUM) {
    if (doc["tip"] == "teslim" && mevcutDurum == ARABA_BEKLENIYOR) {
      addLog("Araba: Teslim tmm");
      mevcutDurum = BEKLEME; sendBantKomut("baslat"); 
    }
  }
}

void kararVer(String renk) {
  unsigned long nowMs = millis();
  bool kolOnline = (nowMs - hbKol < HB_TIMEOUT); bool arabaOnline = (nowMs - hbAraba < HB_TIMEOUT);
  
  if (renk == "kirmizi" || renk == "sari") {
    if (!kolOnline) { addLog("HATA: Kol Offline!");
    mevcutDurum = HATA; return; }
    sonUrunHedef = (renk == "kirmizi") ? "raf_A1" : "yan_kutu"; mevcutDurum = KOL_BEKLENIYOR;
    stateTimer = millis(); sendBantKomut("dur"); sendKolKomut("ite", sonUrunHedef);
  } 
  else if (renk == "mavi" || renk == "yesil") {
    if (!arabaOnline) { addLog("HATA: Araba Offline!");
    mevcutDurum = HATA; return; }
    arabaUrunSayaci++; addLog("Arabaya: " + String(arabaUrunSayaci) + "/5");
    if (arabaUrunSayaci >= 5) {
      sonUrunHedef = "teslimat_noktasi"; sonUrunItildi = false; 
      // 5. Ürün için bantın 2.5 sn daha çalışmasını sağlayan bekleme durumu
      mevcutDurum = ARABA_YUKLENIYOR;
      stateTimer = millis(); sendBantKomut("baslat"); 
    } else { mevcutDurum = BEKLEME; sendBantKomut("baslat");
    }
  } else { mevcutDurum = BEKLEME; sendBantKomut("baslat"); }
}

void durumKontrol() {
  unsigned long nowMs = millis();
  if (mevcutDurum == KOL_BEKLENIYOR && (nowMs - stateTimer > KOL_TIMEOUT_MS)) { mevcutDurum = HATA; stateTimer = nowMs; addLog("HATA: Kol Timeout");
  }
  if (mevcutDurum == TEYIT_BEKLENIYOR && (nowMs - stateTimer > 5000)) { mevcutDurum = HATA;
  stateTimer = nowMs; addLog("HATA: Teyit Timeout"); }
  
  // ARABA YÜKLENİYOR KONTROLÜ (2.5 sn bekle, bantı durdur ve arabayı gönder)
  if (mevcutDurum == ARABA_YUKLENIYOR && (nowMs - stateTimer > 2500)) {
    mevcutDurum = ARABA_BEKLENIYOR; 
    stateTimer = nowMs; 
    sendBantKomut("dur"); 
    sendArabaKomut("git", sonUrunHedef); 
    arabaUrunSayaci = 0; 
  }

  if (mevcutDurum == ARABA_BEKLENIYOR && (nowMs - stateTimer > ARABA_TIMEOUT_MS)) { mevcutDurum = HATA;
  stateTimer = nowMs; addLog("HATA: Araba Timeout"); }
}

void ledVeBuzzerGuncelle() {
  unsigned long nowMs = millis();
  int bagliCihazSayisi = (nowMs - hbBant < HB_TIMEOUT) + (nowMs - hbKol < HB_TIMEOUT) + (nowMs - hbAraba < HB_TIMEOUT);
  digitalWrite(PIN_LED_YESIL, LOW); digitalWrite(PIN_LED_SARI, LOW); digitalWrite(PIN_LED_KIRMIZI, LOW); digitalWrite(PIN_BUZZER, LOW);
  if (mevcutDurum == HATA) { if ((nowMs / 250) % 2 == 0) { digitalWrite(PIN_LED_KIRMIZI, HIGH);
  digitalWrite(PIN_BUZZER, HIGH); } } 
  else if (bagliCihazSayisi == 0) digitalWrite(PIN_LED_KIRMIZI, HIGH);
  else if (bagliCihazSayisi > 0 && bagliCihazSayisi < 3) digitalWrite(PIN_LED_SARI, HIGH);
  else if (bagliCihazSayisi == 3) digitalWrite(PIN_LED_YESIL, HIGH);
}

void ekranGuncelle() {
  display.clearDisplay(); display.setCursor(0, 0);
  unsigned long nowMs = millis();
  String sBant = (nowMs - hbBant < HB_TIMEOUT) ? "ON" : "OFF";
  String sKol = (nowMs - hbKol < HB_TIMEOUT) ? "ON" : "OFF";
  String sAraba = (nowMs - hbAraba < HB_TIMEOUT) ? "ON" : "OFF";
  switch (currentPage) {
    case 0:
      display.println("1/6: GENEL DURUM");
      display.drawLine(0, 10, 128, 10, SH110X_WHITE); display.setCursor(0, 15);
      display.println("Bant:  " + sBant); display.println("Kol:   " + sKol);
      display.println("Araba: " + sAraba); display.print("Sistem: ");
      if(mevcutDurum == BEKLEME) display.println("BEKLEME");
      else if(mevcutDurum == HATA) display.println("HATA!");
      else if(mevcutDurum == TEYIT_BEKLENIYOR) display.println("TEYIT EDIOR..");
      else if(mevcutDurum == ARABA_YUKLENIYOR) display.println("YUKLENIYOR..");
      else display.println("MESGUL");
      break;
    case 1: 
      display.println("2/6: SON URUN"); display.drawLine(0, 10, 128, 10, SH110X_WHITE); display.setCursor(0, 15);
      display.println("Renk : " + sonUrunRenk); display.println("Hedef: " + sonUrunHedef); display.print("Durum: "); display.println(sonUrunItildi ? "Itildi" : "Arabada");
      break;
    case 2: 
      display.println("3/6: BANT DURUMU"); display.drawLine(0, 10, 128, 10, SH110X_WHITE); display.setCursor(0, 15);
      display.println("Motor: " + bantDurum); display.print("Sayac: "); display.println(islenenUrunSayisi);
      break;
    case 3: 
      display.println("4/6: ARABA DURUMU");
      display.drawLine(0, 10, 128, 10, SH110X_WHITE); display.setCursor(0, 15);
      display.println("Komut: " + arabaSonKomut); display.println("Hedef: " + arabaSonHedef); display.print("Kargo: ");
      display.println(String(arabaUrunSayaci) + " / 5 Urun"); 
      break;
    case 4: 
      display.println("5/6: SISTEM LOG");
      display.drawLine(0, 10, 128, 10, SH110X_WHITE); display.setCursor(0, 15);
      display.println("> " + sysLogs[0]); display.println("> " + sysLogs[1]); display.println("> " + sysLogs[2]);
      break;
    case 5:
      display.println("6/6: GELISTIRICILER"); display.drawLine(0, 10, 128, 10, SH110X_WHITE); display.setCursor(0, 15);
      display.println("- Mert Ali Alkan");
      display.println("- Umut Turker"); display.println("- Berk Talha Aslan");
      break;
  }
  display.display();
}

void addLog(String msj) { sysLogs[2] = sysLogs[1];
sysLogs[1] = sysLogs[0]; sysLogs[0] = msj; Serial.println("LOG: " + msj);
}

void publishMessage(const char* topic, StaticJsonDocument<256>& doc, int qos = 1) {
  String jsonStr; serializeJson(doc, jsonStr); mqtt.publish(topic, sifrele(jsonStr).c_str(), qos);
}

void sendBantKomut(String komut) {
  StaticJsonDocument<256> doc; doc["komut"] = komut; publishMessage(TOPIC_BANT_KOMUT, doc);
  bantDurum = (komut == "baslat") ?
"CALISIYOR" : "DURDU";
}
void sendKolKomut(String komut, String hedefKutu) {
  StaticJsonDocument<256> doc; doc["komut"] = komut; doc["hedef_kutu"] = hedefKutu;
doc["timestamp"] = millis(); publishMessage(TOPIC_KOL_KOMUT, doc);
}
void sendArabaKomut(String komut, String hedef) {
  StaticJsonDocument<256> doc; doc["komut"] = komut; doc["hedef"] = hedef;
doc["adim_ms"] = 4000; arabaSonKomut = komut; arabaSonHedef = hedef; publishMessage(TOPIC_ARABA_KOMUT, doc);
}