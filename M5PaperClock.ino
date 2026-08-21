/*
   M5Paper Clock
   Simple clock display with date, WiFi strength, and battery level.
   Uses M5Unified and M5GFX libraries.
*/

#include <M5Unified.h>
#include <M5GFX.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "config.h"
#include "utils.h"
#include "wifi_sync.h"
#include "time_manager.h"
#include "display.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== M5Paper Clock ===");

  // Configure M5
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.internal_imu = false;
  cfg.internal_rtc = true;
  cfg.clear_display = true;
  M5.begin(cfg);
  Serial.println("M5.begin done");

  // WORKAROUND: Fix GPIO27 busy pin pullup for original M5 Paper IT8951E
  // M5GFX may not set this correctly - the EPD busy pin needs a pullup
  gpio_set_pull_mode(GPIO_NUM_27, GPIO_PULLUP_ONLY);
  Serial.println("GPIO27 pullup fix applied");

  CheckRTCEnabled();
  bool rtcValid = ValidateRTCDateTime();
  bool syncRequired = IsSyncRequired(rtcValid);

  Serial.println("Setting power LED...");
  M5.Power.setLed(127);
  Serial.println("Power LED set");

  // Setup display
  if (M5.Display.isEPD()) {
    Serial.println("EPD detected");
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  } else {
    Serial.println("EPD NOT detected - this may be the problem!");
  }
  M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  Serial.printf("Display: %dx%d rotation=%d\n",
                M5.Display.width(), M5.Display.height(), M5.Display.getRotation());

  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setFont(&FreeSansBold12pt7b);
  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.drawString("M5Paper Clock", 20, 20);
  M5.Display.drawString("Booting...", 20, 60);
  M5.Display.endWrite();
  Serial.println("Test screen drawn");

  int rssi = 0;

  if (syncRequired) {
    Serial.println(rtcValid ? "NTP sync required" : "Invalid RTC - syncing NTP");

    M5.Display.startWrite();
    M5.Display.drawString("WiFi connecting...", 20, 100);
    M5.Display.endWrite();

    if (StartWiFi(rssi)) {
      M5.Display.startWrite();
      M5.Display.drawString("NTP syncing...", 20, 140);
      M5.Display.endWrite();

      bool rtcSet = SyncNTPTime();
      StopWiFi();
      if (rtcSet) {
        SaveClockConfigVersion();
      } else {
        Serial.println("NTP sync failed; boot flag not saved");
      }
    } else {
      Serial.println("WiFi failed");
    }
  } else {
    Serial.println("Subsequent boot - using RTC");
  }

  Serial.println("Reading battery level...");
  int batteryLevel = M5.Power.getBatteryLevel();
  Serial.println("Battery level read");
  Serial.printf("Battery: %d%%\n", batteryLevel);

  // Draw the current values once; the next wake runs setup() again.
  Serial.println("Drawing clock...");
  DrawClockDisplay(rssi, batteryLevel);
  Serial.println("Clock drawn");

  // Sleep until the next minute; deep sleep wakes by restarting setup().
  uint32_t sleepSeconds = GetSleepSeconds();

  delay(500);
  Serial.flush();

  M5.Power.setLed(0);
  Serial.println("Entering timed sleep...");
  M5.Power.timerSleep(sleepSeconds);
}

void loop() {
}
