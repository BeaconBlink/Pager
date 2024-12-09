#include <TFT_eSPI.h>
#include <ReactESP.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "ScrollingLine.h"
#include "Settings.h"

#define ADC_PIN 1
#define BUZZER_PIN 5
#define BACKLIGHT_PIN 45
#define LINES_SIZE 3

using namespace reactesp;

// GLOBAL VARIABLES

ReactESP app;

const char* ssid = DEPLOYMENT_SSID;
const char* password = DEPLOYMENT_PASSWORD;
const char* serverUrl = DEPLOYMENT_SERVER_URL;

TFT_eSPI tft = TFT_eSPI();
ScrollingLine lines[LINES_SIZE] = {
  ScrollingLine(&tft, 0, TFT_GREEN, TFT_BLACK, 2),
  ScrollingLine(&tft, lines[0].getBottomY() + 8, TFT_WHITE, TFT_BLACK, 2),
  ScrollingLine(&tft, lines[1].getBottomY() + 8, TFT_RED, TFT_BLACK, 4),
};

JsonDocument jsonDocument;
String serializedJsonDocument;

// FUNCTION DECLARATIONS

void scrollAllLines();
void scanNetworks();
int pingServer();
void onWiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info);
void onWiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
void checkConnectionAndReconnect();
double batteryVoltage();
void setup();
void loop();

// FUNCTION DEFINITIONS

void scrollAllLines() {
  for (uint8_t i = 0; i < LINES_SIZE; i++) {
    lines[i].scrollText();
  }
}

void scanNetworks() {
  static RepeatReaction* scanResultReaction = nullptr;
  static uint8_t failedScanCount = 0;
  const uint8_t maxFailedScans = 3;

  if (scanResultReaction != nullptr) {
    return;
  }

  Serial.println("scanResultReaction initialized");
  lines[1].setText("Scanning");
  WiFi.scanDelete();
  WiFi.scanNetworks(true);

  scanResultReaction = app.onRepeat(1000, [&]() {
    Serial.println("scanResultReaction is running");
    int16_t scanResult = WiFi.scanComplete();

    switch (scanResult) {
      case WIFI_SCAN_FAILED:
        failedScanCount++;
        if (failedScanCount > maxFailedScans) {
          lines[1].setText("Scan failed");
          lines[2].setText("");
          Serial.println("scanResultReaction scan failed");
          goto removeReaction;
        }
        [[fallthrough]];
      case WIFI_SCAN_RUNNING:
        return;
    }
    Serial.println("scanResultReaction scan finished");
    lines[1].setText("Scan finished");
    lines[2].setText("");
    scrollAllLines();

    pingServer();

removeReaction:
    app.remove(scanResultReaction);
    scanResultReaction = nullptr;
    failedScanCount = 0;
    Serial.println("scanResultReaction removed");
  });
}

int pingServer() {
  Serial.println("pingServer started");

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  jsonDocument.clear();
  jsonDocument["mac_address"] = WiFi.macAddress();
  jsonDocument["battery_voltage"] = batteryVoltage();
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

    // TODO: make a function for this or sth
    JsonArray tasks = jsonDocument["tasks"];
    for (JsonObject task : tasks) {
      const char* action = task["action"];
      JsonArray args = task["args"];

      if (strcmp(action, "display") == 0) {
        const char* text = args[0];
        const uint8_t line = args[1];
        lines[line].setText(String(text));

        const uint16_t textColor = args[2];
        lines[line].setTextColor(textColor);

        const uint16_t bgColor = args[3];
        lines[line].setBgColor(bgColor);
      } else if (strcmp(action, "buzz") == 0) {
        const uint8_t buzzCount = args[0];
        const uint16_t buzzLength = args[1];

        for (uint8_t i = 0; i < buzzCount * 2; ++i) {
          app.onDelay(i * buzzLength + 1000, [i]() {
            digitalWrite(BUZZER_PIN, i % 2 == 0);
          });
        }
      }
    }

    Serial.println("pingServer finished running actions");
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();

  lines[1].setText("HTTP (" + String(httpResponseCode) + ")");
  return httpResponseCode;
}

void onWiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Connected to WiFi");
  lines[0].setText("Online");
  lines[0].setTextColor(TFT_GREEN);
}

void onWiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Disonnected from WiFi");
  lines[0].setText("Offline");
  lines[0].setTextColor(TFT_RED);
}

void checkConnectionAndReconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }
}

double batteryVoltage() {
  const double R1 = 200000.0;
  const double R2 = 100000.0;

  const uint32_t adcMilliVolts = analogReadMilliVolts(ADC_PIN);
  const double batteryVoltage = adcMilliVolts / 1000.0 * (R1 + R2) / R2;

  Serial.println("Battery: " + String(batteryVoltage) + "V");

  return batteryVoltage;
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  lines[0].setText("Idle");
  lines[0].setTextColor(TFT_YELLOW);
  lines[1].setText("");
  lines[2].setText("");
  scrollAllLines();

  app.onRepeat(20, scrollAllLines);
  app.onRepeat(30000, checkConnectionAndReconnect);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.onEvent(onWiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(onWiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  WiFi.begin(ssid, password);
  app.onRepeat(30000, scanNetworks);
}

void loop() {
  app.tick();
}
