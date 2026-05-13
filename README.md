# ESP32 Smart Warehouse IoT System

Bu proje, otonom bir akıllı depo sisteminin (Smart Warehouse) uçtan uca IoT uygulamasını içermektedir. Sistem; ürün algılama, robotik kol ile ayrıştırma ve otonom araç (AGV) ile teslimat süreçlerini yöneten 4 ana birimden oluşur.

## 🏗️ Sistem Mimarisi

Sistem, merkezi bir **Kule (Master)** birimi tarafından MQTT protokolü üzerinden koordine edilir. Güvenlik için mesajlar **Dinamik Tuzlama ve Zincirleme XOR** (Dynamic Salt & Chained XOR) yöntemiyle şifrelenir.

### 1. Kule Birimi (Master - ESP32)
- Sistemin beynidir.
- OLED ekran üzerinden tüm birimlerin durumunu (Online/Offline) takip eder.
- Banttan gelen renk verisine göre Kol veya Araba birimlerine komut gönderir.
- Hata durumlarını yönetir ve manuel reset imkanı sunar.

### 2. Bant Birimi (ESP32)
- TCS3200 Renk Sensörü ile ürünleri algılar.
- Dinamik kalibrasyon özelliği ile ortam ışığına uyum sağlar.
- Ürün algıladığı anda bandı durdurur ve Kule'ye bildirim gönderir.

### 3. Robot Kol Birimi (ESP8266)
- Kule'den gelen "ite" komutu ile bant üzerindeki belirli ürünleri (Raf A1/Yan Kutu) ayırır.
- İşlem tamamlandığında Kule'ye geri bildirim göndererek bandın tekrar çalışmasını sağlar.

### 4. Araba Birimi (AGV - ESP8266)
- Kule'den gelen teslimat emirlerini uygular.
- HC-SR04 ultrasonik sensör ile engel algılama özelliğine sahiptir.
- Engel durumunda otonom olarak durur ve sisteme rapor verir.

## 🔐 Güvenlik ve Haberleşme
- **Protokol:** MQTT (TCP/IP)
- **Şifreleme:** Dinamik Tuzlamalı ve Zincirleme XOR (HEX Çıktılı)
- **Güvenlik:** Her mesajda değişen dinamik tuz (salt) kullanımı
- **Veri Formatı:** JSON (ArduinoJson)
- **Heartbeat:** Tüm birimler 5 saniyede bir durum bildirimi gönderir.

## 🚀 Kurulum

1. `config.h` dosyalarındaki Wi-Fi ve MQTT Broker (Raspberry Pi/Mosquitto) bilgilerini güncelleyin.
2. `SECRET_KEY` anahtarının tüm birimlerde aynı olduğundan emin olun.
3. Birimleri sırasıyla (Kule -> Diğerleri) enerjilendirin.
4. Kule üzerindeki OLED ekrandan sistemin "ONLINE" olduğunu doğrulayın.

## 👨‍💻 Geliştiriciler
- Mert Ali Alkan
- Umut Türker
- Berk Talha Aslan

---
*Bu proje eğitim amaçlı geliştirilmiş bir endüstri 4.0 prototipidir.*
