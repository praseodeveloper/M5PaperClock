/*
   M5Paper Clock - Time Manager
   RTC validation, NTP sync, and sleep timing.
*/
#pragma once
#include <esp_sntp.h>
#include <Preferences.h>
#include "config.h"

// Increment when a firmware upload should perform a fresh NTP synchronization.
constexpr uint32_t CLOCK_CONFIG_VERSION = 1;

/* Verify RTC hardware is present; halts if not found */
void CheckRTCEnabled() {
  Serial.println("Checking RTC...");
  if (!M5.Rtc.isEnabled()) {
    Serial.println("RTC not found!");
    for (;;) { delay(1000); }
  }
  Serial.println("RTC enabled");
  Serial.println("RTC initialized by M5.begin");
}

/* Read RTC datetime and validate (year > 2020); returns true if valid */
bool ValidateRTCDateTime() {
  auto rtcDateTime = M5.Rtc.getDateTime();
  bool rtcValid = rtcDateTime.date.year > 2020;
  Serial.printf("RTC: %04d-%02d-%02d %02d:%02d:%02d valid=%d\n",
                rtcDateTime.date.year, rtcDateTime.date.month, rtcDateTime.date.date,
                rtcDateTime.time.hours, rtcDateTime.time.minutes,
                rtcDateTime.time.seconds, rtcValid);
  return rtcValid;
}

/* Check if NTP sync is required based on boot count and RTC validity */
bool IsSyncRequired(bool rtcValid) {
  Preferences preferences;
  preferences.begin("paper-clock", false);
  bool firstBoot = preferences.getUInt("clock-version", 0) != CLOCK_CONFIG_VERSION;
  bool syncRequired = firstBoot || !rtcValid;
  Serial.printf("First boot: %s, NTP sync required: %s\n",
                firstBoot ? "yes" : "no", syncRequired ? "yes" : "no");
  preferences.end();
  return syncRequired;
}

/* Persist clock config version after successful NTP sync */
void SaveClockConfigVersion() {
  Preferences preferences;
  preferences.begin("paper-clock", false);
  preferences.putUInt("clock-version", CLOCK_CONFIG_VERSION);
  preferences.end();
  Serial.println("NTP sync done; RTC initialization saved");
}

/* Sync time via NTP and set the RTC */
bool SyncNTPTime() {
  Serial.println("Syncing NTP...");

  configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

  // Wait for a fresh NTP synchronization, not merely an already-valid RTC/system time.
  uint32_t startMs = millis();
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    if (millis() - startMs > 15000) {
      Serial.println("NTP TIMEOUT");
      return false;
    }
    delay(100);
  }

  struct tm localTime;
  if (!getLocalTime(&localTime, 10000)) {
    Serial.println("NTP TIMEOUT");
    return false;
  }
  Serial.printf("NTP time: %04d-%02d-%02d %02d:%02d:%02d\n",
                localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
                localTime.tm_hour, localTime.tm_min, localTime.tm_sec);

  // M5Unified accepts the local calendar time for the BM8563 RTC.
  M5.Rtc.setDateTime(&localTime);

  auto rtcTime = M5.Rtc.getDateTime();
  Serial.printf("RTC readback: %04d-%02d-%02d %02d:%02d:%02d\n",
                rtcTime.date.year, rtcTime.date.month, rtcTime.date.date,
                rtcTime.time.hours, rtcTime.time.minutes, rtcTime.time.seconds);

  bool rtcSet =
    rtcTime.date.year == localTime.tm_year + 1900 && rtcTime.date.month == localTime.tm_mon + 1 && rtcTime.date.date == localTime.tm_mday && rtcTime.time.hours == localTime.tm_hour && rtcTime.time.minutes == localTime.tm_min;
  Serial.printf("RTC set %s\n", rtcSet ? "OK" : "FAILED");
  return rtcSet;
}

/* Read RTC time and return seconds until the next minute boundary */
uint32_t GetSleepSeconds() {
  m5::rtc_time_t time;
  Serial.println("Reading RTC time for sleep...");
  M5.Rtc.getTime(&time);
  Serial.println("RTC time read");
  uint32_t sleepSeconds = 60 - time.seconds;
  Serial.printf("Sleeping for %lu seconds...\n", sleepSeconds);
  return sleepSeconds;
}
