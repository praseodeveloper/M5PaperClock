/*
   M5Paper Clock
   Simple clock display with date, WiFi strength, and battery level.
   Uses M5Unified and M5GFX libraries.
*/

#include <M5Unified.h>
#include <M5GFX.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>
#include <driver/gpio.h>
#include "config.h"
#include "utils.h"
#include "wifi_sync.h"
#include "time_manager.h"
#include "display.h"

// Any hang longer than this (I2C glitch, EPD busy-wait, RTC halt loop)
// forces a panic reboot so the clock recovers by itself.
constexpr uint32_t WATCHDOG_TIMEOUT_SECONDS = 90;

static void InitWatchdog() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_task_wdt_config_t wdtCfg = {
    .timeout_ms = WATCHDOG_TIMEOUT_SECONDS * 1000U,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  if (esp_task_wdt_init(&wdtCfg) == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_reconfigure(&wdtCfg);
  }
#else
  esp_task_wdt_init(WATCHDOG_TIMEOUT_SECONDS, true);
#endif
  esp_task_wdt_add(NULL);
}

void setup() {
  InitWatchdog();

  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== M5Paper Clock ===");

  // Configure M5
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.internal_imu = false;
  cfg.internal_rtc = true;
  // DrawClockDisplay() already clears the panel; a second full clear on
  // every wake costs extra time and flicker.
  cfg.clear_display = false;
  M5.begin(cfg);
  Serial.println("M5.begin done");

  Serial.printf("Wakeup cause: %d\n", esp_sleep_get_wakeup_cause());

  // WORKAROUND: Fix GPIO27 busy pin pullup for original M5 Paper IT8951E
  // M5GFX may not set this correctly - the EPD busy pin needs a pullup
  gpio_set_pull_mode(GPIO_NUM_27, GPIO_PULLUP_ONLY);
  Serial.println("GPIO27 pullup fix applied");

  // Clear the timer IRQ flag latched by the previous sleep's alarm.
  // A stale/latched INT prevents the next timer alarm from waking the ESP32,
  // which leaves the clock stuck asleep forever.
  M5.Rtc.clearIRQ();

  CheckRTCEnabled();
  bool rtcValid = ValidateRTCDateTime();
  bool syncRequired = IsSyncRequired(rtcValid);

  Serial.println("Setting power LED...");
  M5.Power.setLed(127);
  Serial.println("Power LED set");

  // Setup display
  if (M5.Display.isEPD()) {
    Serial.println("EPD detected");
    // epd_fastest partial refreshes leave residual charge (ghosting), so the
    // default is a full quality waveform; see FULL_REFRESH_EVERY_MINUTES.
    auto dtNow = M5.Rtc.getDateTime();
    bool fullRefresh = FULL_REFRESH_EVERY_MINUTES <= 1 || (dtNow.time.minutes % FULL_REFRESH_EVERY_MINUTES) == 0;
    M5.Display.setEpdMode(fullRefresh ? epd_mode_t::epd_quality : epd_mode_t::epd_fastest);
    Serial.println(fullRefresh ? "EPD mode: quality (full refresh)" : "EPD mode: fastest");
  } else {
    Serial.println("EPD NOT detected - this may be the problem!");
  }
  M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  Serial.printf("Display: %dx%d rotation=%d\n",
                M5.Display.width(), M5.Display.height(), M5.Display.getRotation());

  int rssi = 0;

  if (syncRequired) {
    Serial.println(rtcValid ? "NTP sync required" : "Invalid RTC - syncing NTP");

    // Splash only on sync boots; normal wakes go straight to a single
    // clean DrawClockDisplay update instead of multiple flashing batches.
    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextDatum(TL_DATUM);
    M5.Display.drawString("M5Paper Clock", 20, 20);
    M5.Display.drawString("Booting...", 20, 60);
    M5.Display.drawString(g_syncReason[0] ? g_syncReason : "Syncing time...", 20, 100);
    M5.Display.drawString("WiFi connecting...", 20, 140);
    M5.Display.endWrite();

    if (StartWiFi(rssi)) {
      M5.Display.startWrite();
      M5.Display.drawString("NTP syncing...", 20, 180);
      M5.Display.endWrite();

      bool rtcSet = SyncNTPTime();
      StopWiFi();
      SaveSyncAttempt();
      if (rtcSet) {
        SaveClockConfigVersion();
        g_syncReason = "";
      } else {
        Serial.println("NTP sync failed; boot flag not saved");
        g_syncReason = "NTP failed";
      }
    } else {
      Serial.println("WiFi failed");
      g_syncReason = "WiFi failed";
    }
  } else {
    Serial.println("Subsequent boot - using RTC");
    g_syncReason = "";
  }

  Serial.println("Reading battery level...");
  esp_task_wdt_reset();
  int batteryLevel = M5.Power.getBatteryLevel();
  Serial.println("Battery level read");
  Serial.printf("Battery: %d%%\n", batteryLevel);

  // Draw the current values once; the next wake runs setup() again.
  Serial.println("Drawing clock...");
  DrawClockDisplay(rssi, batteryLevel);
  Serial.println("Clock drawn");

  // Sleep until just after the next minute; deep sleep wakes by restarting setup().
  esp_task_wdt_reset();
  uint32_t sleepSeconds = GetSleepSeconds();

  // Re-arm cleanly: clear any pending IRQ state before the new timer alarm
  // is programmed by timerSleep, otherwise the wake interrupt can be missed.
  M5.Rtc.clearIRQ();

  delay(250);
  Serial.flush();

  M5.Power.setLed(0);
  Serial.println("Entering timed sleep...");
  M5.Power.timerSleep(sleepSeconds);
}

void loop() {
}
