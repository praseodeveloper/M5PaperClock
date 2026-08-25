/*
   M5Paper Clock - Display Functions
   UI drawing using M5GFX/M5Unified.
   Draws directly to M5.Display (no canvas/sprite).
*/
#pragma once
#include <M5GFX.h>
#include "sensors.h"

/* Draw the full clock display */
void DrawClockDisplay(int rssi, int batteryLevel) {
  int w = M5.Display.width();
  int h = M5.Display.height();

  auto dt = M5.Rtc.getDateTime();
  char timeBuf[8];
  char dateBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", dt.time.hours, dt.time.minutes);
  snprintf(dateBuf, sizeof(dateBuf), "%02d.%02d.%04d", dt.date.date, dt.date.month, dt.date.year);

  M5.Display.startWrite();

  // Clear entire screen
  M5.Display.clearDisplay();

  const int frameInset = 6;
  const int frameThickness = 3;
  for (int i = 0; i < frameThickness; ++i) {
    M5.Display.drawRoundRect(frameInset + i, frameInset + i,
                             w - 2 * (frameInset + i),
                             h - 2 * (frameInset + i), 12 - i, TFT_BLACK);
  }

  // Separator line
  M5.Display.fillRect(frameInset + 8, 46, w - 2 * (frameInset + 8), 3, BLACK);

  // Header: Date (left)
  M5.Display.setFont(&DejaVu18);
  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.drawString(dateBuf, 20, 18);

  // Header: WiFi (center)
  //   char wifiBuf[16];
  //   WifiGetRssiAsQuality(rssi, wifiBuf, sizeof(wifiBuf));
  //   M5.Display.setTextDatum(TC_DATUM);
  //   M5.Display.drawString(wifiBuf, w / 2, 18);

  // Header: Battery (right)
  M5.Display.setTextDatum(TR_DATUM);
  char batteryBuf[12];
  snprintf(batteryBuf, sizeof(batteryBuf), "%d%% Bat", batteryLevel);
  M5.Display.drawString(batteryBuf, w - 20, 18);

  // Center: Large time
  M5.Display.setFont(&DejaVu72);
  int32_t timeW = M5.Display.textWidth(timeBuf) + 4;
  int32_t timeH = M5.Display.fontHeight() + 4;
  LGFX_Sprite timeSpr(&M5.Display);
  timeSpr.setColorDepth(16);
  if (timeSpr.createSprite(timeW, timeH)) {
    timeSpr.fillSprite(TFT_WHITE);
    timeSpr.setFont(&DejaVu72);
    timeSpr.setTextColor(TFT_BLACK, TFT_WHITE);
    timeSpr.setTextDatum(TC_DATUM);
    timeSpr.drawString(timeBuf, timeW / 2, 2);
    timeSpr.pushRotateZoomWithAA(w / 2, h / 2 - 24, 0.0f, 2.0f, 2.0f, TFT_WHITE);
    timeSpr.deleteSprite();

    // Rounded frame around the zoomed time (sprite is drawn 2x centered there)
    int32_t boxW = timeW * 2 + 48;
    int32_t boxH = timeH * 2 + 24;
    M5.Display.drawRoundRect(w / 2 - boxW / 2, h / 2 - 24 - boxH / 2,
                             boxW, boxH, 20, TFT_BLACK);
  } else {
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(TC_DATUM);
    M5.Display.drawString(timeBuf, w / 2, h / 2 - 95);
    M5.Display.setTextSize(1);
  }

  // Temperature and humidity, read fresh on every render (each minute).
  // The bundled FreeSans fonts cover ASCII only (0x20-0x7E, no degree
  // glyph), so the degree sign is drawn as a small ring placed inside a
  // two-space gap between the value and the unit.
  float tempC = 0.0f;
  float humidityPct = 0.0f;
  char tempBuf[8];
  char restBuf[16];
  if (ReadTemperatureHumidity(tempC, humidityPct)) {
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", tempC);
    snprintf(restBuf, sizeof(restBuf), "C  %.0f%% RH", humidityPct);
  } else {
    snprintf(tempBuf, sizeof(tempBuf), "--.-");
    snprintf(restBuf, sizeof(restBuf), "C  --%% RH");
  }
  char climateBuf[24];
  snprintf(climateBuf, sizeof(climateBuf), "%s  %s", tempBuf, restBuf);

  int16_t climateY = h / 2 + 105;
  M5.Display.setFont(&DejaVu24);
  M5.Display.setTextDatum(TC_DATUM);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.drawString(climateBuf, w / 2, climateY);

  int16_t slotLeft =
    w / 2 - M5.Display.textWidth(climateBuf) / 2 + M5.Display.textWidth(tempBuf);
  int16_t ringX = slotLeft + M5.Display.textWidth(" ");
  // Top-aligning the ring with the digit tops puts it in superscript pose.
  int16_t ringY = climateY + 3;
  M5.Display.drawCircle(ringX, ringY, 3, TFT_BLACK);
  M5.Display.drawCircle(ringX, ringY, 2, TFT_BLACK);

  // Bottom: Battery voltage
  //   int16_t batVolt = M5.Power.getBatteryVoltage();
  //   M5.Display.setFont(&FreeSansBold12pt7b);
  //   M5.Display.setTextSize(1);
  //   M5.Display.setTextDatum(TC_DATUM);
  //   char voltageBuf[12];
  //   snprintf(voltageBuf, sizeof(voltageBuf), "%d mV", batVolt);
  //   M5.Display.drawString(voltageBuf, w / 2, h - 40);

  if (g_syncReason[0]) {
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.drawString(g_syncReason, w / 2, h - 75);
  }

  M5.Display.endWrite();
}
