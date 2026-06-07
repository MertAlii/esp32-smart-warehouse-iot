#ifndef CRYPTO_H
#define CRYPTO_H

#include <Arduino.h>
#include "config.h"

// Tüm .ino dosyalarının en üstünde tanımlanan canlı şalter değişkeni
extern bool sifrelemeAktif; 

char toHex(uint8_t val) {
    if (val < 10) return '0' + val;
    return 'A' + (val - 10);
}

uint8_t fromHex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

String sifrele(String jsonMesaj) {
    // Şalter kapalıysa saf metni geri gönder! (AKILLI KONTROL)
    if (!sifrelemeAktif) return jsonMesaj; 
    
    if (jsonMesaj.length() == 0) return "";
    String output = "";
    int keyLen = strlen(SECRET_KEY);
    uint8_t salt = random(0, 256); 
    
    output += toHex(salt >> 4);
    output += toHex(salt & 0x0F);
    
    for (int i = 0; i < jsonMesaj.length(); i++) {
        uint8_t c = jsonMesaj[i];
        uint8_t k = SECRET_KEY[i % keyLen];
        uint8_t encryptedChar = c ^ k ^ salt;
        output += toHex(encryptedChar >> 4);
        output += toHex(encryptedChar & 0x0F);
        salt = (salt + k) % 256; 
    }
    return output;
}

String sifreCoz(String hexPayload) {
    // Şalter kapalıysa şifre çözmeye çalışma, metin zaten saftır! (AKILLI KONTROL)
    if (!sifrelemeAktif) return hexPayload; 

    if (hexPayload.length() < 2) return "";
    String output = "";
    int keyLen = strlen(SECRET_KEY);
    uint8_t salt = (fromHex(hexPayload[0]) << 4) | fromHex(hexPayload[1]);
    
    int charIndex = 0;
    for (int i = 2; i < hexPayload.length(); i += 2) {
        uint8_t encryptedChar = (fromHex(hexPayload[i]) << 4) | fromHex(hexPayload[i+1]);
        uint8_t k = SECRET_KEY[charIndex % keyLen];
        char c = encryptedChar ^ salt ^ k;
        output += c;
        salt = (salt + k) % 256;
        charIndex++;
    }
    return output;
}

#endif // CRYPTO_H