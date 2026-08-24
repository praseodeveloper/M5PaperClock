/*
   M5Paper Clock - Time Manager
   RTC validation, NTP sync, and sleep timing.
*/
#pragma once
#include <esp_sntp.h>
#include <ctime>
#include <WiFi.h>
#include <Preferences.h>
#include "config.h"

// Increment when a firmware upload should perform a fresh NTP synchronization.
constexpr uint32_t CLOCK_CONFIG_VERSION = 1;

// NVS keys must stay <= 15 characters (ESP-IDF NVS hard limit).
constexpr const char* NVS_KEY_VERSION = "clock-version";
constexpr const char* NVS_KEY_LAST_SYNC = "lastsyncmin";
constexpr const char* NVS_KEY_LAST_TRY = "lasttrymin";

// Wake this many seconds after the minute boundary. Keeps the timer alarm
// far enough in the future that it can never fire before deep sleep is
// actually entered (a race that permanently kills the wake interrupt).
constexpr uint32_t WAKE_OFFSET_SECONDS = 2;

// Why the current boot wants a sync; shown on the splash/final screen.
const char* g_syncReason = "";

// Survives deep sleep (and watchdog resets) independent of NVS, so the
// sync-attempt throttle still works if flash storage misbehaves.
RTC_DATA_ATTR uint32_t s_rtcLastTryMinutes = 0;
// Mirror of the last successful sync time; compared against NVS after each
// wake to detect flash storage that does not survive reboots.
RTC_DATA_ATTR uint32_t s_rtcMirrorSyncMinutes = 0;
RTC_DATA_ATTR uint8_t s_rtcMirrorValid = 0;

/* Verify RTC hardware is present; halts if not found */
void CheckRTCEnabled() {
  Serial.println("Checking RTC...");
  if (!M5.Rtc.isEnabled()) {
    Serial.println("RTC not found!");
    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setFont(&FreeSansBold12pt7b);
    M5.Display.setTextDatum(TL_DATUM);
    M5.Display.drawString("RTC not found!", 20, 20);
    M5.Display.endWrite();
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

/* Convert RTC datetime to minutes elapsed since the Unix epoch (timezone-free) */
static uint32_t DateTimeToTotalMinutes(const m5::rtc_datetime_t& dt) {
  long y = dt.date.year;
  long m = dt.date.month;
  long d = dt.date.date;
  if (m <= 2) {
    y--;
    m += 12;
  }
  long era = y / 400;
  long yoe = y - era * 400;
  long doy = (153 * (m - 3) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = era * 146097 + doe - 719468;
  return (uint32_t)(days * 1440L + dt.time.hours * 60L + dt.time.minutes);
}

/* Check if NTP sync is required: invalid RTC, config version change,
   or the resync interval has elapsed (the BM8563 drifts noticeably).
   When a due sync keeps failing, attempts are throttled to
   SYNC_RETRY_MINUTES so the WiFi splash does not appear on every wake. */
bool IsSyncRequired(bool rtcValid) {
  if (!rtcValid) {
    g_syncReason = "RTC invalid";
    Serial.println("Invalid RTC - NTP sync required");
    return true;
  }

  bool versionMismatch;
  uint32_t lastSyncMinutes;
  uint32_t lastTryMinutes;
  {
    Preferences preferences;
    preferences.begin("paper-clock", true);
    versionMismatch = preferences.getUInt(NVS_KEY_VERSION, 0) != CLOCK_CONFIG_VERSION;
    lastSyncMinutes = preferences.getUInt(NVS_KEY_LAST_SYNC, 0);
    lastTryMinutes = preferences.getUInt(NVS_KEY_LAST_TRY, 0);
    preferences.end();
  }

  // Post-wake mirror comparison: proves whether NVS content actually
  // survives reboots. If it does not, treat the state as untrustworthy.
  if (s_rtcMirrorValid && lastSyncMinutes != s_rtcMirrorSyncMinutes) {
    Serial.println("WARNING: NVS lost its sync state across reboot - flash storage is not persisting!");
    versionMismatch = true;
  }

  uint32_t nowMinutes = DateTimeToTotalMinutes(M5.Rtc.getDateTime());
  uint32_t effectiveLastTry = lastTryMinutes > s_rtcLastTryMinutes ? lastTryMinutes : s_rtcLastTryMinutes;
  Serial.printf("NVS: v-match=%d last-sync=%lu (mirror=%lu valid=%d) last-try=%lu (rtc=%lu) now=%lu\n",
                !versionMismatch ? 1 : 0,
                (unsigned long)lastSyncMinutes, (unsigned long)s_rtcMirrorSyncMinutes,
                (unsigned)s_rtcMirrorValid,
                (unsigned long)lastTryMinutes, (unsigned long)s_rtcLastTryMinutes,
                (unsigned long)nowMinutes);

  uint32_t maxAgeMinutes = NTP_RESYNC_INTERVAL_HOURS * 60UL;
  bool overdue = versionMismatch || lastSyncMinutes == 0 || nowMinutes < lastSyncMinutes || (nowMinutes - lastSyncMinutes) >= maxAgeMinutes;
  if (!overdue) {
    Serial.printf("Last NTP sync %lu minutes ago (interval %lu) - not due\n",
                  (unsigned long)(nowMinutes - lastSyncMinutes),
                  (unsigned long)maxAgeMinutes);
    return false;
  }

  // Throttle applies to every overdue cause, including storage problems,
  // so a dead NVS can no longer trigger a sync attempt on every wake.
  if (effectiveLastTry != 0 && nowMinutes >= effectiveLastTry && (nowMinutes - effectiveLastTry) < SYNC_RETRY_MINUTES) {
    Serial.printf("Overdue but last attempt was %lu minutes ago - retrying in %lu\n",
                  (unsigned long)(nowMinutes - effectiveLastTry),
                  (unsigned long)(SYNC_RETRY_MINUTES - (nowMinutes - effectiveLastTry)));
    return false;
  }

  if (versionMismatch) {
    g_syncReason = s_rtcMirrorValid ? "Storage error" : "Firmware updated";
    Serial.println("Sync state invalid/stale - NTP sync required");
  } else if (lastSyncMinutes == 0 || nowMinutes < lastSyncMinutes) {
    g_syncReason = "No sync record";
    Serial.println("No valid last-sync timestamp - NTP sync required");
  } else {
    g_syncReason = "Scheduled resync";
    Serial.println("NTP resync due");
  }
  return true;
}

/* Record that a sync attempt just happened (regardless of outcome).
   RTC memory is the authoritative throttle store; NVS is best-effort. */
void SaveSyncAttempt() {
  s_rtcLastTryMinutes = DateTimeToTotalMinutes(M5.Rtc.getDateTime());
  Preferences preferences;
  if (!preferences.begin("paper-clock", false)) {
    Serial.println("NVS open FAILED (attempt stamp)");
    return;
  }
  size_t written = preferences.putUInt(NVS_KEY_LAST_TRY, s_rtcLastTryMinutes);
  preferences.end();
  Serial.printf("Attempt stamp written: %u bytes\n", (unsigned)written);
}

/* Persist clock config version and last-sync timestamp after successful NTP sync.
   Verifies strictly against the expected value; on failure clears the
   namespace and retries once (recovers from a damaged NVS namespace). */
void SaveClockConfigVersion() {
  uint32_t syncMinutes = DateTimeToTotalMinutes(M5.Rtc.getDateTime());
  s_rtcMirrorSyncMinutes = syncMinutes;
  s_rtcMirrorValid = 1;
  s_rtcLastTryMinutes = syncMinutes;

  for (int attempt = 1; attempt <= 2; ++attempt) {
    Preferences preferences;
    if (!preferences.begin("paper-clock", false)) {
      Serial.println("NVS open FAILED (sync state)");
      g_syncReason = "Storage error";
      return;
    }
    size_t writtenVersion = preferences.putUInt(NVS_KEY_VERSION, CLOCK_CONFIG_VERSION);
    size_t writtenSync = preferences.putUInt(NVS_KEY_LAST_SYNC, syncMinutes);
    preferences.end();
    Serial.printf("NVS write attempt %d: version=%u bytes, sync-minutes=%u bytes\n",
                  attempt, (unsigned)writtenVersion, (unsigned)writtenSync);

    Preferences check;
    bool opened = check.begin("paper-clock", true);
    uint32_t storedVersion = opened ? check.getUInt(NVS_KEY_VERSION, 0) : 0;
    uint32_t storedSync = opened ? check.getUInt(NVS_KEY_LAST_SYNC, 0) : 0;
    if (opened) {
      check.end();
    }
    bool ok = opened && storedVersion == CLOCK_CONFIG_VERSION && storedSync == syncMinutes;
    Serial.printf("Save verify %s (v=%lu sync=%lu, expected %lu)\n",
                  ok ? "OK" : "FAILED",
                  (unsigned long)storedVersion, (unsigned long)storedSync,
                  (unsigned long)syncMinutes);
    if (ok) {
      g_syncReason = "";
      Serial.println("NTP sync done; sync state saved");
      return;
    }
    if (attempt == 1) {
      Serial.println("Verify failed - clearing namespace and retrying once");
      Preferences wipe;
      if (wipe.begin("paper-clock", false)) {
        wipe.clear();
        wipe.end();
      } else {
        Serial.println("NVS open FAILED for wipe");
        break;
      }
    }
  }
  g_syncReason = "Storage error";
}
/* Wait until SNTP reports a completed sync */
static bool WaitForSntpSync(uint32_t timeoutMs) {
  uint32_t startMs = millis();
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    if (millis() - startMs > timeoutMs) {
      return false;
    }
    delay(100);
  }
  return true;
}

/* Push the synced system time into the RTC and verify the write */
static bool SetRtcFromSystemTime() {
  struct tm localTime;
  if (!getLocalTime(&localTime, 10000)) {
    Serial.println("No valid system time after sync");
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

  struct tm rtcReadback = {};
  rtcReadback.tm_year = rtcTime.date.year - 1900;
  rtcReadback.tm_mon = rtcTime.date.month - 1;
  rtcReadback.tm_mday = rtcTime.date.date;
  rtcReadback.tm_hour = rtcTime.time.hours;
  rtcReadback.tm_min = rtcTime.time.minutes;
  rtcReadback.tm_sec = rtcTime.time.seconds;
  // mktime must deduce DST from the date on both sides; comparing a
  // zero-initialized isdst against localtime_r's flag skews summer-time
  // comparisons by exactly one hour.
  localTime.tm_isdst = -1;
  rtcReadback.tm_isdst = -1;
  time_t sysEpoch = mktime(&localTime);
  time_t rtcEpoch = mktime(&rtcReadback);
  bool rtcSet = sysEpoch != (time_t)-1 && rtcEpoch != (time_t)-1 && (rtcEpoch > sysEpoch ? rtcEpoch - sysEpoch : sysEpoch - rtcEpoch) <= 120;
  Serial.printf("RTC set %s\n", rtcSet ? "OK" : "FAILED");
  return rtcSet;
}

/* Sync time via NTP and set the RTC.
   Tries the configured hostnames first; if they do not answer (DNS failure
   or UDP/123 filtered), retries with numeric-IP anycast servers that
   require no DNS. */
bool SyncNTPTime() {
  Serial.println("Syncing NTP...");

  IPAddress resolved;
  const char* servers[] = { NTP_SERVER1, NTP_SERVER2, NTP_SERVER3 };
  for (const char* server : servers) {
    if (WiFi.hostByName(server, resolved) == 1) {
      Serial.printf("DNS: %s -> %s\n", server, resolved.toString().c_str());
    } else {
      Serial.printf("DNS FAILED for %s\n", server);
    }
  }

  configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
  if (WaitForSntpSync(12000)) {
    return SetRtcFromSystemTime();
  }

  Serial.println("Configured servers unreachable - trying numeric-IP fallback");
  configTzTime(TZ_INFO, "162.159.200.1", "216.239.35.0", "129.6.15.28");
  if (WaitForSntpSync(10000)) {
    return SetRtcFromSystemTime();
  }

  Serial.println("NTP TIMEOUT on all servers");
  g_syncReason = "NTP failed";
  return false;
}

/* Read RTC time and return seconds to sleep so we wake just after the
   next minute boundary. Result is always within [WAKE_OFFSET_SECONDS+1, 60+WAKE_OFFSET_SECONDS]. */
uint32_t GetSleepSeconds() {
  auto dt = M5.Rtc.getDateTime();
  uint32_t secondsIntoMinute = dt.time.seconds;
  if (secondsIntoMinute > 59) {
    secondsIntoMinute = 59;
  }
  uint32_t sleepSeconds = (60 - secondsIntoMinute) + WAKE_OFFSET_SECONDS;
  Serial.printf("RTC second=%u, sleeping for %lu seconds...\n",
                (unsigned)secondsIntoMinute, (unsigned long)sleepSeconds);
  return sleepSeconds;
}
