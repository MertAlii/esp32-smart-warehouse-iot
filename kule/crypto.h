#ifndef CRYPTO_H
#define CRYPTO_H

#include <Arduino.h>
#include "config.h"

// Sadece ESP32 kullanıldığında mbedtls kütüphanelerini dahil et
#if defined(ESP32)
  #include <mbedtls/aes.h>
  #include <mbedtls/base64.h>
#endif

/**
 * @brief PKCS#7 standardına göre metnin sonuna padding (dolgu) ekler.
 */
String pkcs7Pad(String input) {
    int blockSize = 16;
    int padLen = blockSize - (input.length() % blockSize);
    String padded = input;
    for (int i = 0; i < padLen; i++) {
        padded += (char)padLen; 
    }
    return padded;
}

/**
 * @brief PKCS#7 dolgusunu çözer ve temiz metni verir.
 */
String pkcs7Unpad(String input) {
    if (input.length() == 0) return input;
    int padLen = input[input.length() - 1]; 
    if (padLen > 0 && padLen <= 16) {
        return input.substring(0, input.length() - padLen);
    }
    return input; 
}

/**
 * @brief JSON string'i AES-128-CBC ile şifreler, başa IV ekler ve Base64'e çevirir.
 */
String sifrele(String jsonMesaj) {
    #if DEBUG_NO_ENCRYPTION == 1
        return jsonMesaj; // Test modu aktifse şifrelemeden gönder
    #else
        #if defined(ESP32)
            String paddedMsg = pkcs7Pad(jsonMesaj);
            int inputLen = paddedMsg.length();
            unsigned char output[inputLen];

            unsigned char iv[16];
            unsigned long currentMs = millis();
            for (int i = 0; i < 16; i++) {
                iv[i] = (currentMs >> (i % 4)) ^ (0xAA + i); 
            }
            
            unsigned char iv_copy[16];
            memcpy(iv_copy, iv, 16);

            mbedtls_aes_context aes;
            mbedtls_aes_init(&aes);
            mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);
            mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, inputLen, iv_copy, (const unsigned char*)paddedMsg.c_str(), output);
            mbedtls_aes_free(&aes);

            int combinedLen = 16 + inputLen;
            unsigned char combined[combinedLen];
            memcpy(combined, iv, 16);                 
            memcpy(combined + 16, output, inputLen);  

            size_t b64Len = 0;
            mbedtls_base64_encode(NULL, 0, &b64Len, combined, combinedLen); 
            unsigned char b64Output[b64Len];
            mbedtls_base64_encode(b64Output, b64Len, &b64Len, combined, combinedLen); 

            return String((char*)b64Output);
        #elif defined(ESP8266)
            // ESP8266 için şifreleme kütüphanesi aktif değil, ham veri gönderiliyor.
            return jsonMesaj;
        #endif
    #endif
}

/**
 * @brief Base64 payload'u alır, çözer.
 */
String sifreCoz(String b64Payload) {
    #if DEBUG_NO_ENCRYPTION == 1
        return b64Payload; // Test modu aktifse işlemi atla[cite: 2]
    #else
        #if defined(ESP32)
            size_t decodedLen = 0;
            mbedtls_base64_decode(NULL, 0, &decodedLen, (const unsigned char*)b64Payload.c_str(), b64Payload.length());
            if (decodedLen == 0) return "";
            
            unsigned char decoded[decodedLen];
            mbedtls_base64_decode(decoded, decodedLen, &decodedLen, (const unsigned char*)b64Payload.c_str(), b64Payload.length());

            if (decodedLen <= 16) return ""; 

            unsigned char iv[16];
            memcpy(iv, decoded, 16);
            int cipherLen = decodedLen - 16;
            unsigned char ciphertext[cipherLen];
            memcpy(ciphertext, decoded + 16, cipherLen);
            unsigned char output[cipherLen];

            mbedtls_aes_context aes;
            mbedtls_aes_init(&aes);
            mbedtls_aes_setkey_dec(&aes, AES_KEY, 128);
            mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipherLen, iv, ciphertext, output);
            mbedtls_aes_free(&aes);

            String decryptedStr = "";
            for (int i = 0; i < cipherLen; i++) {
                decryptedStr += (char)output[i];
            }

            return pkcs7Unpad(decryptedStr);
        #elif defined(ESP8266)
            // ESP8266 için şifre çözme kütüphanesi aktif değil, ham veri işleniyor.
            return b64Payload;
        #endif
    #endif
}

#endif // CRYPTO_H