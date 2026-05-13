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

// --- Güvenlik (AES-128-CBC) ---
// 16 byte (128 bit) anahtar. Tüm ESP32 birimlerinde aynı olmalı!
const uint8_t AES_KEY[16] = {
  0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x18,
  0x29, 0x3A, 0x4B, 0x5C, 0x6D, 0x7E, 0x8F, 0x90
};

// 1 yapıldığında şifreleme bypass edilir, ham JSON gönderilir/alınır. (Testler için)
#define DEBUG_NO_ENCRYPTION 1

// --- Zamanlama Sabitleri (Milisaniye) ---
#define KOL_TIMEOUT_MS 10000      // Kol işlem süresi aşımı (10 sn)
#define ARABA_TIMEOUT_MS 30000    // Araba teslimat süresi aşımı (30 sn)
#define RECONNECT_DELAY 5000      // Bağlantı koptuğunda tekrar deneme gecikmesi
#define HEARTBEAT_INTERVAL 5000   // Heartbeat gönderme aralığı (5 sn)

#endif // CONFIG_H