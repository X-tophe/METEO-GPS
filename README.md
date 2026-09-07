# METEO-GPS
Ministation Meteo Aero

Station météo portable pour l'aéronautique légère (planeur/ULM), basée sur ESP32.
Elle mesure température, humidité et pression via un capteur BME280, recale la
pression avec l'altitude GPS, calcule le point de rosée et affiche un état météo
(dégagé, brouillard, pluie, volable, humide) sur un écran LCD I2C et le port série.

## Matériel

- ESP32 (utilise `HardwareSerial` UART1 et `Wire` pour l'I2C)
- Capteur BME280 (I2C, adresse `0x76`)
- Module GPS UART (ex. NEO-6M) sur les broches RX 5 / TX 6, 9600 bauds
- Écran LCD 20x4 I2C (adresse `0x27`)

## Dépendances (Arduino/PlatformIO)

- [Adafruit BME280 Library](https://github.com/adafruit/Adafruit_BME280_Library)
- [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor)
- [hd44780](https://github.com/duinoWitchery/hd44780)
- [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus)

## Fonctionnement

Toutes les 15 secondes, le sketch relit les capteurs, recalcule la météo et met à
jour l'affichage LCD ainsi que la sortie série (mesures + informations GPS :
date/heure UTC et locale, position, altitude, nombre de satellites, HDOP).
