#ifndef CRYPTO_H
#define CRYPTO_H

#include <Arduino.h>
#include "config.h"

// --- HEX DÖNÜŞÜM YARDIMCILARI (Kayıpsız iletim için) ---
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

/**
 * @brief JSON string'i Dinamik Tuz (Salt) ve Zincirleme XOR ile şifreler, HEX olarak döndürür.
 */
String sifrele(String jsonMesaj) {
    #if DEBUG_NO_ENCRYPTION == 1
        return jsonMesaj; // Test modu
    #else
        if (jsonMesaj.length() == 0) return "";
        
        String output = "";
        int keyLen = strlen(SECRET_KEY);
        
        // 1. Adım: Rastgele bir Tuz (Salt) oluştur. (Her mesajda farklı şifre çıkmasını sağlar)
        uint8_t salt = random(0, 256); 
        
        // Şifreli metnin en başına tuzu gizliyoruz ki alıcı taraf çözebilsin
        output += toHex(salt >> 4);
        output += toHex(salt & 0x0F);
        
        // 2. Adım: Zincirleme XOR Şifreleme
        for (int i = 0; i < jsonMesaj.length(); i++) {
            uint8_t c = jsonMesaj[i];
            uint8_t k = SECRET_KEY[i % keyLen];
            
            // Karakteri hem gizli anahtarla hem de o anki tuzla harmanla
            uint8_t encryptedChar = c ^ k ^ salt;
            
            // Şifreli veriyi HEX olarak ekle
            output += toHex(encryptedChar >> 4);
            output += toHex(encryptedChar & 0x0F);
            
            // Zincirleme Mantığı: Bir sonraki karakter için tuzu anahtara göre değiştir
            // Böylece şifreleme algoritması sürekli form değiştirir.
            salt = (salt + k) % 256; 
        }
        return output;
    #endif
}

/**
 * @brief HEX formatındaki şifreli payload'u alır ve orijinal JSON'a geri çevirir.
 */
String sifreCoz(String hexPayload) {
    #if DEBUG_NO_ENCRYPTION == 1
        return hexPayload; // Test modu
    #else
        if (hexPayload.length() < 2) return "";
        
        String output = "";
        int keyLen = strlen(SECRET_KEY);
        
        // 1. Adım: Metnin en başından (ilk 2 HEX karakterden) ilk tuzu çekip al
        uint8_t salt = (fromHex(hexPayload[0]) << 4) | fromHex(hexPayload[1]);
        
        // 2. Adım: Geri kalan HEX veriyi çöz
        int charIndex = 0;
        for (int i = 2; i < hexPayload.length(); i += 2) {
            // İki HEX karakteri birleştirip 1 byte (şifreli karakter) elde et
            uint8_t encryptedChar = (fromHex(hexPayload[i]) << 4) | fromHex(hexPayload[i+1]);
            uint8_t k = SECRET_KEY[charIndex % keyLen];
            
            // Geriye Dönüş Mantığı: Şifreli karakteri tuz ve anahtarla çöz
            char c = encryptedChar ^ salt ^ k;
            output += c;
            
            // Tuzu şifreleme tarafıyla birebir aynı şekilde güncelle ki döngü kopmasın
            salt = (salt + k) % 256;
            charIndex++;
        }
        return output;
    #endif
}

#endif // CRYPTO_H