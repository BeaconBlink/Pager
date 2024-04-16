#include <TFT_eSPI.h>
#include <ReactESP.h>
#include "ScrollingLine.h"

#define BACKLIGHT_PIN 45
#define LINES_SIZE 3

using namespace reactesp;

ReactESP app;

TFT_eSPI tft = TFT_eSPI();
ScrollingLine lines[LINES_SIZE] = {
  ScrollingLine(&tft, 0, TFT_GREEN, TFT_BLACK, 2),
  ScrollingLine(&tft, lines[0].getBottomY(), TFT_WHITE, TFT_BLACK, 3),
  ScrollingLine(&tft, lines[1].getBottomY(), TFT_RED, TFT_BLACK, 5),
};

void scrollAllLines() {
  for (int i = 0; i < LINES_SIZE; i++) {
    lines[i].scrollText();
  }
}

void setup() {
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  lines[0].setText("Connected");
  lines[1].setText("Some generic message or something");
  lines[2].setText("VERY IMPORTANT MESSAGE");

  RepeatReaction *scrollReaction = app.onRepeat(20, scrollAllLines);
}

void loop() {
  app.tick();
}
