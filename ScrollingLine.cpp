#include "ScrollingLine.h"

ScrollingLine::ScrollingLine(TFT_eSPI* tft, int32_t lineY, uint16_t textColor, uint16_t bgColor, uint8_t textSize)
  : lineSprite(tft), cursorX(0), lineY(lineY), textColor(textColor), bgColor(bgColor), textSize(textSize), scrollSpeed(textSize) {
  lineSprite.setColorDepth(8);
  lineSprite.createSprite(160, textSize * 8);
  lineSprite.fillSprite(bgColor);
  lineSprite.setTextColor(textColor, bgColor);
  lineSprite.setTextSize(textSize);
  lineSprite.setTextWrap(false);
}

void ScrollingLine::setText(String newText) {
  text = newText;
  textWidth = lineSprite.textWidth(text);

  int16_t horizontalMargin = lineSprite.width() - textWidth;
  cursorX = horizontalMargin > 0 ? horizontalMargin / 2 : 0;
  scrollSpeed = horizontalMargin > 0 ? 0 : textSize;
}

void ScrollingLine::setBgColor(uint16_t newBgColor) {
  bgColor = newBgColor;
  lineSprite.setTextColor(textColor, newBgColor);
}

void ScrollingLine::setTextColor(uint16_t newTextColor) {
  textColor = newTextColor;
  lineSprite.setTextColor(newTextColor, bgColor);
}

void ScrollingLine::scrollText() {
  lineSprite.fillSprite(bgColor);
  lineSprite.setCursor(cursorX, 0);
  lineSprite.print(text);
  lineSprite.pushSprite(0, lineY);

  cursorX -= scrollSpeed;
  if (cursorX <= -textWidth) {
    cursorX = lineSprite.width();
  }
}

int ScrollingLine::getBottomY() {
  return lineY + lineSprite.height();
}
