#include <Wire.h>
#include <Adafruit_BMP085.h>

Adafruit_BMP085 bmp;

void sensor() {
  if (!bmp.begin()) {
    sensorBMP = "Помилка ініціалізації";
    Serial.println("Помилка ініціалізації BMP085!");
    temperature = 0;
    pressure = 0;
  } else {
    temperature = bmp.readTemperature();
    pressure = bmp.readPressure();
  }
}
