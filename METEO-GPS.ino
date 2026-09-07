#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

/* ---------- CONFIG ---------- */
#define BME_ADDR          0x76
#define GPS_RX_PIN        5
#define GPS_TX_PIN        6
#define GPS_BAUD          9600
#define LCD_ADDR          0x27
#define LCD_COLS          20
#define LCD_ROWS          4
#define READ_INTERVAL_MS  15000UL
#define GPS_TIMEOUT_MS    5000UL
#define GPS_MIN_CHARS     10
#define LOCALT_OFST       2
#define BME_TEMP_OFFSET   (-2.0f)
#define BME_PRESS_OFFSET  (-4.0f)
#define ALT_PRESS_FACTOR  (8.5f)

/* ---------- OBJETS ---------- */
Adafruit_BME280    bme;
TinyGPSPlus        gps;
hd44780_I2Cexp     disp(LCD_ADDR, LCD_COLS, LCD_ROWS);
HardwareSerial     hs(1);

/* ---------- TIMERS ---------- */
unsigned long tRead      = 0;
bool          gpsChecked = false;

/* ---------- DONNÉES ---------- */
float temperature, humidity, pressure, dew;
int   diff;

/* ---------- ENUM METEO ---------- */
enum Meteo {
  METEO_CLEAR,
  METEO_FOG,
  METEO_RAIN,
  METEO_FLYABLE,
  METEO_HUMID
};
Meteo currentMeteo;

/* ---------- STRUCT HEURE LOCALE ---------- */
struct LocalTime {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

/* ---------- POINT DE ROSÉE ---------- */
float dewPoint(float temp, float hum) {
  const float a = 17.27f;
  const float b = 237.7f;
  float alpha = ((a * temp) / (b + temp)) + logf(hum / 100.0f);
  return (b * alpha) / (a - alpha);
}

/* ---------- LECTURE CAPTEUR ---------- */
void readSensors() {
  temperature = bme.readTemperature() + BME_TEMP_OFFSET;
  humidity    = bme.readHumidity();
  float altCorr = gps.altitude.isValid()
                  ? (gps.altitude.meters() / ALT_PRESS_FACTOR)
                  : 0.0f;
  pressure = (bme.readPressure() / 100.0f) + altCorr + BME_PRESS_OFFSET;
  dew  = dewPoint(temperature, humidity);
  diff = (int)(temperature - dew);
}

/* ---------- LOGIQUE METEO ---------- */
Meteo computeMeteo(float humidity, int diff) {
  if (humidity >= 98)                               return METEO_RAIN;
  if (diff <= 2 && humidity >= 96)                  return METEO_FOG;
  if (diff > 3  && humidity <= 75)                  return METEO_FLYABLE;
  if (diff <= 3 && humidity >= 90 && humidity < 96) return METEO_HUMID;
  return METEO_CLEAR;
}

/* ---------- HEURE LOCALE ---------- */
LocalTime getLocalTime(const TinyGPSPlus& gps) {
  LocalTime lt;
  long totalSeconds = gps.time.hour()   * 3600L
                    + gps.time.minute() * 60L
                    + gps.time.second()
                    + LOCALT_OFST       * 3600L;
  totalSeconds = ((totalSeconds % 86400L) + 86400L) % 86400L;
  lt.hour   = totalSeconds / 3600;
  lt.minute = (totalSeconds % 3600) / 60;
  lt.second = totalSeconds % 60;
  return lt;
}

/* ---------- SERIAL ---------- */
void printSerial() {
  Serial.printf("Température : %.1f °C\n",  temperature);
  Serial.printf("Pression    : %.1f hPa\n", pressure);
  Serial.printf("Humidité    : %.1f %%\n",  humidity);
  Serial.printf("Point rosée : %.1f °C\n",  dew);
  Serial.printf("Écart T-Td  : %d °C\n",    diff);
  const char* meteoStr[] = {
    "Dégagé", "Brouillard", "Pluie", "Volable", "Humide++"
  };
  Serial.printf("Météo       : %s\n\n", meteoStr[currentMeteo]);
}

/* ---------- AFFICHAGE LCD ---------- */
void lcdDisp() {
  char buf[LCD_COLS + 1];
  disp.clear();

  snprintf(buf, sizeof(buf), "QNH %6.0f hPa", pressure);
  disp.setCursor(0, 0); disp.print(buf);

  snprintf(buf, sizeof(buf), "Temp: %4.0f ", temperature);
  disp.setCursor(0, 1); disp.print(buf);
  disp.write(223); disp.print("C");

  snprintf(buf, sizeof(buf), "Humid: %3.0f %%", humidity);
  disp.setCursor(0, 2); disp.print(buf);

  float   altM = gps.altitude.isValid()  ? gps.altitude.meters()    : 0.0f;
  uint8_t sats = gps.satellites.isValid() ? gps.satellites.value()  : 0;
  snprintf(buf, sizeof(buf), "Alt:%6.0f m  Sat:%2u", altM, sats);
  disp.setCursor(0, 3); disp.print(buf);
}

/* ---------- AFFICHAGE GPS SERIAL ---------- */
void displayGps() {

  // Date
  Serial.print(F("Date/Heure : "));
  if (gps.date.isValid()) {
    Serial.printf("%02u/%02u/%04u", gps.date.day(), gps.date.month(), gps.date.year());
  } else {
    Serial.print(F("Wait"));
  }

  // Heure UTC + heure locale
  Serial.print(F(" - "));
  if (gps.time.isValid()) {
    Serial.printf("%02u:%02u:%02u UTC", gps.time.hour(), gps.time.minute(), gps.time.second());
    LocalTime lt = getLocalTime(gps);
    Serial.printf("  ->  %02u:%02u:%02u Local\n", lt.hour, lt.minute, lt.second);
  } else {
    Serial.println(F("Wait"));
  }

  // Position
  Serial.print(F("Position   : "));
  if (gps.location.isValid()) {
    double lat = gps.location.lat();
    double lng = gps.location.lng();
    Serial.printf("%.4f %c , %.4f %c\n",
                  lat, lat >= 0 ? 'N' : 'S',
                  lng, lng >= 0 ? 'E' : 'W');
  } else {
    Serial.println(F("Wait"));
  }

  // Altitude
  Serial.print(F("Altitude   : "));
  if (gps.altitude.isValid()) {
    Serial.printf("%.1f m\n", gps.altitude.meters());
  } else {
    Serial.println(F("Wait"));
  }

  // Satellites
  Serial.print(F("Satellites : "));
  if (gps.satellites.isValid()) {
    Serial.println(gps.satellites.value());
  } else {
    Serial.println(F("Wait"));
  }

  // HDOP
  Serial.print(F("HDOP       : "));
  if (gps.hdop.isValid()) {
    Serial.println(gps.hdop.hdop());
  } else {
    Serial.println(F("Wait"));
  }

  Serial.println();
}  // ← fermeture correcte de displayGps()

/* ---------- SETUP ---------- */
void setup() {
  Serial.begin(9600);
  hs.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Wire.begin();
  disp.init();
  disp.backlight();

  if (!bme.begin(BME_ADDR)) {
    disp.setCursor(0, 0);
    disp.print(F("BME280 introuvable!"));
    Serial.println(F("Erreur : BME280 non détecté."));
    while (1) delay(100);
  }

  readSensors();
  currentMeteo = computeMeteo();
  lcdDisp();
  printSerial();
  displayGps();
}

/* ---------- LOOP ---------- */
void loop() {
  while (hs.available() > 0) {
    gps.encode(hs.read());
  }

  if (!gpsChecked && millis() > GPS_TIMEOUT_MS) {
    gpsChecked = true;
    if (gps.charsProcessed() < GPS_MIN_CHARS) {
      Serial.println(F("Erreur : GPS non détecté, vérifiez le câblage."));
      disp.setCursor(0, 0);
      disp.print(F("GPS introuvable!    "));
    }
  }

  unsigned long now = millis();
  if (now - tRead >= READ_INTERVAL_MS) {
    tRead = now;
    readSensors();
    currentMeteo = computeMeteo(humidity, diff);
    printSerial();
    displayGps();
    lcdDisp();
  }
}
