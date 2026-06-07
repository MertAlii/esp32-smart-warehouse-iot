import paho.mqtt.client as mqtt
import time
import os

# Windows CMD'de özel renk kodlarını (ANSI) aktif etmek için
os.system("color")

# Sistem Renk Paleti
RESET = "\033[0m"
CYAN = "\033[96m"    # Kule Rengi
YELLOW = "\033[93m"  # Bant Rengi
MAGENTA = "\033[95m" # Kol Rengi
BLUE = "\033[94m"    # Araba Rengi
RED = "\033[91m"     # Ayar ve Uyarı Rengi
GREEN = "\033[92m"   # Sistem Mesajları

MQTT_BROKER = "192.168.4.1"
MQTT_PORT = 1883

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print(f"{GREEN}[SİSTEM] Ag baglantisi saglandi. Gercek zamanli log monitoru aktif...{RESET}\n")
        client.subscribe("akillidepo/#")
    else:
        print(f"{RED}[HATA] Baglanti kurulamadi! Hata Kodu: {reason_code}{RESET}")

def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        payload = msg.payload.decode('utf-8')
    except:
        payload = str(msg.payload)

    # Konuya (Topic) göre renk seçimi
    if "kule" in topic: color = CYAN
    elif "bant" in topic: color = YELLOW
    elif "kol" in topic: color = MAGENTA
    elif "araba" in topic: color = BLUE
    elif "ayar/sifreleme" in topic: color = RED
    else: color = RESET

    timestamp = time.strftime("%H:%M:%S")
    # Çıktı Formatı: [Saat] [Konu] Mesaj
    print(f"\033[90m[{timestamp}] {color}[{topic}] {RESET}{payload}")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, "Log_Monitor_01")
client.on_connect = on_connect
client.on_message = on_message

try:
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_forever()
except KeyboardInterrupt:
    print(f"\n{RED}[SİSTEM] Log monitoru durduruldu. Menuye donuluyor...{RESET}")
except Exception as e:
    print(f"{RED}[HATA] Ag baglantisi koptu: {e}{RESET}")   