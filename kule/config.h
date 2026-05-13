#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Wi-Fi Ayarları ---
#define WIFI_SSID "AkilliDepo_Network"
#define WIFI_PASSWORD "depo2025"

// --- MQTT Broker Ayarları ---
#define MQTT_BROKER "192.168.4.1" // Raspberry Pi IP adresi
#define MQTT_PORT 1883

// Her birim kendi .ino dosyasında "client_id" değişkenini tanımlayacak
extern const char* client_id; 

// --- Gelişmiş Şifreleme Anahtarı ---
// Sistemdeki tüm cihazlar bu anahtarı bilmelidir. İstediğin gibi değiştirebilirsin.
const char* SECRET_KEY = "Akilli_Depo_2026_MUB!"; 

// 1 yapıldığında şifreleme bypass edilir, ham JSON gönderilir/alınır. (Testler için)
#define DEBUG_NO_ENCRYPTION 1

// --- Zamanlama Sabitleri (Milisaniye) ---
#define KOL_TIMEOUT_MS 10000      // Kol işlem süresi aşımı (10 sn)
#define ARABA_TIMEOUT_MS 30000    // Araba teslimat süresi aşımı (30 sn)
#define RECONNECT_DELAY 5000      // Bağlantı koptuğunda tekrar deneme gecikmesi
#define HEARTBEAT_INTERVAL 5000   // Heartbeat gönderme aralığı (5 sn)

#endif // CONFIG_H