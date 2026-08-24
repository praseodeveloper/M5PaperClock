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

  // Separator line
  M5.Display.drawLine(0, 55, w, 55, BLACK);

  // Header: Date (left)
  M5.Display.setFont(&FreeSansBold12pt7b);
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
  M5.Display.setFont(&FreeSansBold24pt7b);
  M5.Display.setTextSize(3);
  M5.Display.setTextDatum(TC_DATUM);
  M5.Display.drawString(timeBuf, w / 2, h / 2 - 65);

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
  M5.Display.setFont(&FreeSansBold12pt7b);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(TC_DATUM);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.drawString(climateBuf, w / 2, climateY);

  int16_t slotLeft =
    w / 2 - M5.Display.textWidth(climateBuf) / 2 + M5.Display.textWidth(tempBuf);
  int16_t ringX = slotLeft + M5.Display.textWidth(" ");
  // TC_DATUM renders digits from y+1 to y+17 (baseline = y + font ascent 17),
  // so top-aligning the ring with the digit tops puts it in superscript pose.
  int16_t ringY = climateY + 1 + 3;
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
