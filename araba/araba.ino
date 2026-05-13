// ESP8266 - L298N Motor Test Kodu

// Pin Tanımlamaları (Bir önceki şemaya göre)
#define ENA D1  // Sağ motor hızı
#define IN1 D2  // Sağ motor yön 1
#define IN2 D3  // Sağ motor yön 2

#define IN3 D4  // Sol motor yön 1
#define IN4 D5  // Sol motor yön 2
#define ENB D6  // Sol motor hızı

// Hız ayarı (ESP8266'da analogWrite varsayılan olarak 0-1023 arası değer alır)
// 1023 tam hızdır, pillerin tam doluysa çok hızlı dönebilir. 
// Test için 600 gibi orta bir değer kullanıyoruz.
int motorHizi = 600; 

void setup() {
  Serial.begin(115200);
  Serial.println("\nMotor Testi Başlıyor...");

  // Bütün pinleri ÇIKIŞ olarak ayarlıyoruz
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Başlangıçta motorların durduğundan emin olalım
  dur();
  delay(2000); // 2 saniye bekle
}

void loop() {
  Serial.println("İLERİ");
  ileri();
  delay(2000);

  Serial.println("DUR");
  dur();
  delay(1000);

  Serial.println("GERİ");
  geri();
  delay(2000);

  Serial.println("DUR");
  dur();
  delay(1000);

  Serial.println("SAĞA DÖN");
  sagaDon();
  delay(1000);

  Serial.println("DUR");
  dur();
  delay(1000);

  Serial.println("SOLA DÖN");
  solaDon();
  delay(1000);

  Serial.println("TEST DÖNGÜSÜ BİTTİ, TEKRARLIYOR...");
  dur();
  delay(3000);
}

// --- YÖN FONKSİYONLARI ---

void ileri() {
  analogWrite(ENA, motorHizi);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  
  analogWrite(ENB, motorHizi);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void geri() {
  analogWrite(ENA, motorHizi);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  
  analogWrite(ENB, motorHizi);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void sagaDon() {
  // Sağa dönmek için sol motor ileri, sağ motor geri döner
  analogWrite(ENA, motorHizi);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  
  analogWrite(ENB, motorHizi);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void solaDon() {
  // Sola dönmek için sağ motor ileri, sol motor geri döner
  analogWrite(ENA, motorHizi);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  
  analogWrite(ENB, motorHizi);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void dur() {
  analogWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  
  analogWrite(ENB, 0);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}