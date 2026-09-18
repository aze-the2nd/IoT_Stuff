// Toolchain smoke test: confirm PlatformIO can build and flash C++ firmware
// onto the CYD board over /dev/ttyUSB0, and that the built-in ILI9341
// display is wired up correctly. No sensor logic yet.

#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.println("IoT-Stuff");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 60);
  tft.println("Hello, RTD sensor!");

  Serial.println("Boot OK: display initialized.");
}

void loop() {
  static uint32_t lastTick = 0;
  if (millis() - lastTick >= 1000) {
    lastTick = millis();
    Serial.printf("uptime: %lus\n", millis() / 1000);
  }
}
