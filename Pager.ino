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

#if DEBUG
#define DEBUG_FUNCTION(func) func
#else
#define DEBUG_FUNCTION(func) ((void)0)
#endif

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
  const uint8_t maxFailedScans = 5;

  if (scanResultReaction != nullptr) {
    return;
  }

  DEBUG_FUNCTION(Serial.println("scanResultReaction initialized"));
  DEBUG_FUNCTION(lines[1].setText("Scanning"));
  DEBUG_FUNCTION(scrollAllLines());

  WiFi.scanDelete();
  WiFi.scanNetworks(true);

  scanResultReaction = app.onRepeat(1000, [&]() {
    DEBUG_FUNCTION(Serial.println("scanResultReaction is running"));
    int16_t scanResult = WiFi.scanComplete();

    switch (scanResult) {
      case WIFI_SCAN_FAILED:
        failedScanCount++;
        if (failedScanCount > maxFailedScans) {
          DEBUG_FUNCTION(lines[1].setText("Scan failed"));
          lines[2].setText("");
          DEBUG_FUNCTION(Serial.println("scanResultReaction scan failed"));
          goto removeReaction;
        }
        [[fallthrough]];
      case WIFI_SCAN_RUNNING:
        return;
    }
    DEBUG_FUNCTION(Serial.println("scanResultReaction scan finished"));
    DEBUG_FUNCTION(lines[1].setText("Scan finished"));
    DEBUG_FUNCTION(scrollAllLines());
    lines[2].setText("");
    scrollAllLines();

    pingServer();

removeReaction:
    app.remove(scanResultReaction);
    scanResultReaction = nullptr;
    failedScanCount = 0;
    DEBUG_FUNCTION(Serial.println("scanResultReaction removed"));
  });
}

int pingServer() {
  DEBUG_FUNCTION(Serial.println("pingServer started"));

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  jsonDocument.clear();
  jsonDocument["mac_address"] = WiFi.macAddress();
  jsonDocument["battery_voltage"] = batteryVoltage();
  jsonDocument["battery_percentage"] = 69;
  JsonArray scanResults = jsonDocument.createNestedArray("scan_results");

  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    JsonObject scanResult = scanResults.createNestedObject();
    scanResult["ssid"] = WiFi.SSID(i);
    scanResult["rssi"] = WiFi.RSSI(i);
    scanResult["bssid"] = WiFi.BSSIDstr(i);
  }

  serializeJson(jsonDocument, serializedJsonDocument);
  DEBUG_FUNCTION(Serial.print("HTTP POST body: "));
  DEBUG_FUNCTION(Serial.println(serializedJsonDocument));

  int httpResponseCode = http.POST(serializedJsonDocument);
  if (httpResponseCode > 0) {
    DEBUG_FUNCTION(Serial.print("HTTP Response "));
    DEBUG_FUNCTION(Serial.print(httpResponseCode));
    DEBUG_FUNCTION(Serial.print(": "));
    serializedJsonDocument = http.getString();
    DEBUG_FUNCTION(Serial.println(serializedJsonDocument));
    deserializeJson(jsonDocument, serializedJsonDocument);

    // TODO: make a function for this or sth
    JsonArray tasks = jsonDocument["tasks"];
    for (JsonObject task : tasks) {
      const char* action = task["action"];
      JsonArray args = task["args"];

      if (strcmp(action, "display") == 0) {
        const char* text = args[0];
        const uint8_t line = args[1];
        const uint16_t textColor = args[2];
        const uint16_t bgColor = args[3];

        lines[line].setText(String(text));
        lines[line].setTextColor(textColor);
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

    DEBUG_FUNCTION(Serial.println("pingServer finished running actions"));
  } else {
    DEBUG_FUNCTION(Serial.print("Error code: "));
    DEBUG_FUNCTION(Serial.println(httpResponseCode));
  }
  http.end();

  if (httpResponseCode != 200) {
    lines[0].setTextColor(TFT_YELLOW);
  }

  DEBUG_FUNCTION(lines[1].setText("HTTP: " + String(httpResponseCode)));
  DEBUG_FUNCTION(scrollAllLines());
  return httpResponseCode;
}

void onWiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  DEBUG_FUNCTION(Serial.println("Connected to WiFi"));
  lines[0].setText("Online");
  lines[0].setTextColor(TFT_GREEN);
}

void onWiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  DEBUG_FUNCTION(Serial.println("Disonnected from WiFi"));
  lines[0].setText("Offline");
  lines[0].setTextColor(TFT_RED);
}

void checkConnectionAndReconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }
}

double batteryVoltage() {
  const uint8_t numReadings = 10;

  DEBUG_FUNCTION(Serial.println("Battery voltage measurements initialized"));

  uint32_t totalMilliVolts = 0;
  for (uint8_t i = 0; i < numReadings; i++) {
    uint32_t adcMilliVolts = analogReadMilliVolts(ADC_PIN) * VOLTAGE_MULTIPLIER;
    DEBUG_FUNCTION(Serial.println("ADC: " + String(adcMilliVolts) + "mV"));
    totalMilliVolts += adcMilliVolts;
    delay(10);
  }

  uint32_t averageMilliVolts = totalMilliVolts / numReadings;
  uint32_t averageBatteryMilliVolts = averageMilliVolts * 3;
  double averageBatteryVolatage = averageBatteryMilliVolts / 1000.0;

  DEBUG_FUNCTION(lines[1].setText("BAT: "+ String(averageBatteryVolatage) + "V"));
  DEBUG_FUNCTION(scrollAllLines());
  DEBUG_FUNCTION(Serial.println("Battery: " + String(averageBatteryVolatage) + "V"));
  return averageBatteryVolatage;
}

void setup() {
  DEBUG_FUNCTION(Serial.begin(115200));
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
