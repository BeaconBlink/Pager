#include <TFT_eSPI.h>
#include <ReactESP.h>
#include <WiFi.h>
#include "ScrollingLine.h"
#include "Settings.h"

#define BACKLIGHT_PIN 45
#define LINES_SIZE 3

using namespace reactesp;

ReactESP app;

const char* ssid = DEPLOYMENT_SSID;
const char* password = DEPLOYMENT_PASSWORD;
const char* serverUrl = DEPLOYMENT_SERVER_URL;

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
  static RepeatReaction* scanResultReaction = nullptr;
  static int failedScanCount = 0;
  const int maxFailedScans = 3;

  if (scanResultReaction != nullptr) {
    return;
  }

  Serial.println("scanResultReaction initialized");
  lines[1].setText("Scanning...");
  WiFi.scanDelete();
  WiFi.scanNetworks(true);

  scanResultReaction = app.onRepeat(1000, [&]() {
    Serial.println("scanResultReaction is running");
    int scanResult = WiFi.scanComplete();

    switch (scanResult) {
      case WIFI_SCAN_FAILED:
        failedScanCount++;
        if (failedScanCount > maxFailedScans) {
          lines[1].setText("Scan Failed");
          lines[2].setText("");
          Serial.println("scanResultReaction scan failed");
          goto removeReaction;
        }
        [[fallthrough]];
      case WIFI_SCAN_RUNNING:
        return;
    }

    lines[1].setText("Scan Done");
    lines[2].setText(String(scanResult) + " networks");

removeReaction:
    app.remove(scanResultReaction);
    scanResultReaction = nullptr;
    failedScanCount = 0;
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
