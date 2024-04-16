#include <TFT_eSPI.h>
#include <ReactESP.h>
#include <WiFi.h>
#include "ScrollingLine.h"

#define BACKLIGHT_PIN 45
#define LINES_SIZE 3

using namespace reactesp;

ReactESP app;

TFT_eSPI tft = TFT_eSPI();
ScrollingLine lines[LINES_SIZE] = {
  ScrollingLine(&tft, 0, TFT_GREEN, TFT_BLACK, 2),
  ScrollingLine(&tft, lines[0].getBottomY() + 8, TFT_WHITE, TFT_BLACK, 2),
  ScrollingLine(&tft, lines[1].getBottomY() + 8, TFT_RED, TFT_BLACK, 2),
};

void scrollAllLines() {
  for (int i = 0; i < LINES_SIZE; i++) {
    lines[i].scrollText();
  }
}

void scanAndShow() {
  static RepeatReaction *scanResultReaction = nullptr;
  if (scanResultReaction != nullptr) {
    return;
  }

  Serial.println("scanResultReaction initialized");
  lines[1].setText("Scanning...");
  WiFi.scanNetworks(true);

  scanResultReaction = app.onRepeat(1000, [&]() {
    Serial.println("scanResultReaction is running");
    int scanResult = WiFi.scanComplete();
    if (scanResult == WIFI_SCAN_RUNNING || scanResult == WIFI_SCAN_FAILED) {
      return;
    }

    lines[1].setText("Scan Done");
    lines[2].setText((String)scanResult + " networks");
    WiFi.scanDelete();
    app.remove(scanResultReaction);
    scanResultReaction = nullptr;
    Serial.println("scanResultReaction removed");
  });
}


void setup() {
  Serial.begin(115200);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  lines[0].setText("Initialized");
  lines[1].setText("");
  lines[2].setText("");

  app.onRepeat(20, scrollAllLines);
  app.onDelay(0, scanAndShow);
  app.onRepeat(30000, scanAndShow);
}

void loop() {
  app.tick();
}
