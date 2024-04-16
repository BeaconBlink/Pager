#ifndef ScrollingLine_h
#define ScrollingLine_h

#include <TFT_eSPI.h>

class ScrollingLine {
private:
  TFT_eSprite lineSprite;
  String text;
  int16_t cursorX, scrollSpeed, textWidth;
  int32_t lineY;
  uint8_t textSize;
  uint16_t textColor, bgColor;

public:
  ScrollingLine(TFT_eSPI* tft, int32_t lineY, uint16_t textColor, uint16_t bgColor, uint8_t textSize);
  void setText(String newText);
  void setBgColor(uint16_t newBgColor);
  void setTextColor(uint16_t newTextColor);
  void scrollText();
  int getBottomY();
};

#endif
