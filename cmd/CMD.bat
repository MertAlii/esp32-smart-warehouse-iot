@echo off
:: Windows 10/11 CMD'de ANSI Renk Kodlarini aktif etmek icin sihirli kod
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do (
  set ESC=%%b
)

title Akilli Depo - Sistem Yonetici Paneli
color 0F
mode con: cols=85 lines=26

:menu
cls
echo %ESC%[96m===================================================================================%ESC%[0m
echo %ESC%[96m
echo       ##    #  #  #  #    #    #     ####   ####  ####  #### 
echo      #  #   # #   #  #    #    #     #   #  #     #  #  #  #
echo     ######  ##    #  #    #    #     #   #  ###   ####  #  #
echo     #    #  # #   #  #    #    #     #   #  #     #     #  #
echo     #    #  #  #  #  #### #### #     ####   ####  #     ####
echo %ESC%[0m
echo %ESC%[96m===================================================================================%ESC%[0m
echo %ESC%[97m                       [ SISTEM YONETICI PANELI - v2.1 ]%ESC%[0m
echo.
echo    %ESC%[97m[%ESC%[92m1%ESC%[97m] %ESC%[92m[+] SIFRELEMEYI AKTIF ET%ESC%[90m  (Guvenli Iletisim Modu)%ESC%[0m
echo    %ESC%[97m[%ESC%[91m2%ESC%[97m] %ESC%[91m[-] SIFRELEMEYI KAPAT%ESC%[90m     (Bakim ve Test Modu)%ESC%[0m
echo    %ESC%[97m[%ESC%[93m3%ESC%[97m] %ESC%[93m[*] CANLI LOG MONITORU%ESC%[90m    (Sistem Ag Trafigini Izle)%ESC%[0m
echo    %ESC%[97m[%ESC%[95m4%ESC%[97m] %ESC%[95m[x] CIKIS%ESC%[0m
echo.
echo %ESC%[96m===================================================================================%ESC%[0m
set /p secim="%ESC%[96m admin@akillidepo:~$ %ESC%[0m"

if "%secim%"=="1" goto ac
if "%secim%"=="2" goto kapat
if "%secim%"=="3" goto dinle
if "%secim%"=="4" goto exit
goto menu

:ac
cls
echo %ESC%[96m===================================================================================%ESC%[0m
echo %ESC%[93m[*] Sistem yapilandirmasi guncelleniyor...%ESC%[0m
timeout /t 1 >nul
echo %ESC%[93m[*] Guvenli iletisim modulleri hazirlaniyor...%ESC%[0m
timeout /t 1 >nul
python -c "import paho.mqtt.publish as publish; publish.single('akillidepo/ayar/sifreleme', '1', hostname='192.168.4.1', retain=True)"
echo %ESC%[92m[+] BASARILI: Sifreleme modulu aktif edildi. Tum cihazlar guvenli modda calisiyor.%ESC%[0m
echo %ESC%[96m===================================================================================%ESC%[0m
pause
goto menu

:kapat
cls
echo %ESC%[96m===================================================================================%ESC%[0m
echo %ESC%[91m[!] DIKKAT: Sistem bakim moduna aliniyor...%ESC%[0m
timeout /t 1 >nul
echo %ESC%[91m[!] Iletisim sifrelemesi devre disi birakiliyor...%ESC%[0m
timeout /t 1 >nul
python -c "import paho.mqtt.publish as publish; publish.single('akillidepo/ayar/sifreleme', '0', hostname='192.168.4.1', retain=True)"
echo %ESC%[91m[-] BASARILI: Sifreleme kapatildi. Sistem test (duz metin) moduna gecis yapti.%ESC%[0m
echo %ESC%[96m===================================================================================%ESC%[0m
pause
goto menu

:dinle
cls
echo %ESC%[96m===================================================================================%ESC%[0m
echo %ESC%[93m[*] Canli Log Monitoru baslatiliyor...%ESC%[0m
echo %ESC%[93m[*] Ana menuye donmek icin CTRL+C basip 'N' (Hayir) secin.%ESC%[0m
echo %ESC%[96m===================================================================================%ESC%[0m
timeout /t 1 >nul
python 01.py
pause
goto menu