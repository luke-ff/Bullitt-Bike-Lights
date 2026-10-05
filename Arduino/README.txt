Bullitt-Bike-Lights - ESP8266

Projektstruktur
---------------
Bullitt-Bike-Lights.ino
Controller.h
WebServer.h
WebServer.cpp
data/index.html

WLAN
----
SSID:      Bullitt-Light
Passwort:  bullitt123
Webseite:  http://192.168.4.1

Benötigte Arduino-Libraries
---------------------------
- FastLED
- ESP8266WiFi (ESP8266 Core)
- ESP8266WebServer (ESP8266 Core)
- LittleFS (ESP8266 Core)

LittleFS
--------
Die Datei data/index.html muss in das LittleFS-Dateisystem des ESP8266
hochgeladen werden.

Arduino IDE:
Je nach ESP8266-Core-Version erfolgt das über das LittleFS Data Upload
Werkzeug bzw. über das entsprechende ESP8266 LittleFS Plugin.

Wichtig
-------
Dieses Projekt ist auf den ESP8266 als WLAN-Access-Point ausgelegt.
Es benötigt keinen vorhandenen WLAN-Router und keinen Internetzugang.

Die Webserver-Schnittstelle verwendet Controller.h, damit WebServer.cpp
nicht direkt auf die internen Variablen des LED-Controllers zugreifen muss.
