@echo off
title Akilli Depo - Siber Komuta Merkezi
color 0A

:menu
cls
echo ==================================================
echo         AKILLI DEPO SIBER KONTROL MERKEZI
echo ==================================================
echo [1] Sifrelemeyi AC (Guvenli Mod)
echo [2] Sifrelemeyi KAPAT (Test Modu)
echo [3] Ag Trafigni Dinle (Renkli Canli Akis)
echo [4] Cikis
echo ==================================================
set /p secim="Islem numarasi girin (1-4): "

if "%secim%"=="1" goto ac
if "%secim%"=="2" goto kapat
if "%secim%"=="3" goto dinle
if "%secim%"=="4" goto exit

:ac
cls
echo [SISTEM] Sifreleme protokolu aktif ediliyor...
python -c "import paho.mqtt.publish as publish; publish.single('akillidepo/ayar/sifreleme', '1', hostname='192.168.4.1', retain=True)"
echo [BASARILI] Tum cihazlara sifrelemeyi AC emri (1) kalici olarak gonderildi!
pause
goto menu

:kapat
cls
echo [SISTEM] Sifreleme protokolu devre disi birakiliyor...
python -c "import paho.mqtt.publish as publish; publish.single('akillidepo/ayar/sifreleme', '0', hostname='192.168.4.1', retain=True)"
echo [BASARILI] Tum cihazlara sifrelemeyi KAPAT emri (0) kalici olarak gonderildi!
pause
goto menu

:dinle
cls
echo [SISTEM] Canli dinleme baslatiliyor... (Menuye donmek icin CTRL+C basin)
python 01.py
pause
goto menu