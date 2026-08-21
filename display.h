/*
   M5Paper Clock - Display Functions
   UI drawing using M5GFX/M5Unified.
   Draws directly to M5.Display (no canvas/sprite).
*/
#pragma once
#include <M5GFX.h>
// #include <Wire.h>
// SHT30 support is temporarily disabled.

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
  // M5.Display.setTextDatum(TC_DATUM);
  // M5.Display.drawString(WifiGetRssiAsQuality(rssi) + "% WiFi", w / 2, 18);

  // Header: Battery (right)
  M5.Display.setTextDatum(TR_DATUM);
  M5.Display.drawString(String(batteryLevel) + "% Bat", w - 20, 18);

  // Center: Large time
  M5.Display.setFont(&FreeSansBold24pt7b);
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(TC_DATUM);
  M5.Display.drawString(timeBuf, w / 2, h / 2 - 65);

  // Temperature and humidity display is temporarily disabled.

  // Bottom: Battery voltage
  int16_t batVolt = M5.Power.getBatteryVoltage();
  M5.Display.setFont(&FreeSansBold12pt7b);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(TC_DATUM);
  M5.Display.drawString(String(batVolt) + " mV", w / 2, h - 40);

  M5.Display.endWrite();
}
