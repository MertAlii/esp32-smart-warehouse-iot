#ifndef TOPICS_H
#define TOPICS_H

// --- MQTT Topic Sabitleri ---

// Banttan Kuleye
#define TOPIC_BANT_URUN    "akillidepo/bant/urun_bilgisi"

// Kuleden Diğer Birimlere Komutlar
#define TOPIC_KOL_KOMUT    "akillidepo/kule/komut/kol"
#define TOPIC_ARABA_KOMUT  "akillidepo/kule/komut/araba"
#define TOPIC_BANT_KOMUT   "akillidepo/kule/komut/bant"

// Diğer Birimlerden Kuleye Durum Bildirimleri
#define TOPIC_KOL_TAMAM    "akillidepo/kol/islem_tamam"
#define TOPIC_ARABA_DURUM  "akillidepo/araba/durum"

/* * NOT: Heartbeat topic'leri dinamik olarak oluşturulacaktır.
 * Örn: "akillidepo/bant/heartbeat", "akillidepo/kule/heartbeat"
 * Kule, abonelik için "akillidepo/+/heartbeat" wildcard'ını kullanır.
 */

#endif // TOPICS_H