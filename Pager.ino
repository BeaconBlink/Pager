#include <TFT_eSPI.h>
#include <ReactESP.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
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

JsonDocument jsonDocument;
String serializedJsonDocument;
int pingServer() {
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  jsonDocument.clear();
  jsonDocument["mac_address"] = WiFi.macAddress();
  JsonArray scanResults = jsonDocument.createNestedArray("scan_results");

  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    JsonObject scanResult = scanResults.createNestedObject();
    scanResult["ssid"] = WiFi.SSID(i);
    scanResult["rssi"] = WiFi.RSSI(i);
    scanResult["bssid"] = WiFi.BSSIDstr(i);
  }

  serializeJson(jsonDocument, serializedJsonDocument);
  Serial.print("HTTP POST body: ");
  Serial.println(serializedJsonDocument);

  int httpResponseCode = http.POST(serializedJsonDocument);
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response ");
    Serial.print(httpResponseCode);
    Serial.print(": ");
    serializedJsonDocument = http.getString();
    Serial.println(serializedJsonDocument);
    deserializeJson(jsonDocument, serializedJsonDocument);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();

  return httpResponseCode;
}

void connect() {
  static RepeatReaction* wifiConnectionReaction = nullptr;
  static int failedConnectionCount = 0;
  const int maxFailedConnections = 5;

  if (wifiConnectionReaction != nullptr) {
    return;
  }

  Serial.println("wifiConnectionReaction initialized");
  lines[1].setText("Connecting...");
  WiFi.begin(ssid, password);

  wifiConnectionReaction = app.onRepeat(1000, [&]() {
    Serial.println("wifiConnectionReaction is running");

    if (WiFi.status() != WL_CONNECTED) {
      failedConnectionCount++;
      if (failedConnectionCount > maxFailedConnections) {
        lines[1].setText("Connection failed");
        goto removeReaction;
      }
      return;
    }

    lines[1].setText("Connected");
    pingServer();

    // TODO: do things according to server response
    // for now just initialize a scan :v
    app.onDelay(0, scanAndShow);

removeReaction:
    app.remove(wifiConnectionReaction);
    wifiConnectionReaction = nullptr;
    failedConnectionCount = 0;
    Serial.println("wifiConnectionReaction removed");
    lines[1].setText("Disconnected");

    WiFi.disconnect();
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
  app.onDelay(0, connect);
  app.onRepeat(30000, connect);
}

void loop() {
  app.tick();
}
