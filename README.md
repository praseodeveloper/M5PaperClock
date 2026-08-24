### A simple NTP based clock for M5 Paper
```
https://docs.m5stack.com/en/core/m5paper_v1.1
```

The clock never runs `loop()`. Every minute the device wakes from deep sleep,
reboots into `setup()`, draws the current time once, and goes back to sleep.
This keeps power draw minimal and makes every cycle a clean fresh boot.

## Boot cycle (runs on every wake)

1. **Arm watchdog** — a 90 s task watchdog panics and reboots the device if any
   step hangs (I²C glitch, EPD busy-wait, etc.), so the clock self-recovers.
2. **Initialize** — `M5.begin()` (RTC enabled, display not pre-cleared),
   log the wake-up cause, apply the GPIO27 pull-up workaround for the IT8951,
   and clear any latched BM8563 timer-IRQ flag from the previous sleep
   (a stale flag would prevent the next wake).
3. **Check RTC** — verify the BM8563 responds; on failure show an error on the
   display and halt (the watchdog reboots to retry).
4. **Validate RTC time** — considered valid if year > 2020.
5. **Decide whether an NTP sync is required.** A sync happens when:
   - the RTC time is invalid, or
   - the stored config version differs from `CLOCK_CONFIG_VERSION`
     (e.g. after a firmware update), or
   - there is no valid last-sync record, or
   - the last successful sync is older than `NTP_RESYNC_INTERVAL_HOURS`.
   Attempts are throttled to one per `SYNC_RETRY_MINUTES` using RTC memory
   (survives deep sleep even if flash storage misbehaves), so a failing sync
   never blocks the per-minute display updates.
6. **If syncing** (splash screen shows the reason):
   - Connect Wi-Fi (`config.h`, credentials are not written to flash;
     15 s timeout). On success continue, on failure mark `WiFi failed`.
   - Resolve the configured NTP hostnames (`config.h`) and log the results.
   - Try those servers for 12 s; if unanswered, retry with hard-coded
     numeric-IP fallback servers (Cloudflare / Google / NIST) that need no DNS.
   - Set the BM8563 RTC from the received time and verify the read-back
     (±120 s tolerance, DST-aware comparison).
   - Persist `clock-version` and the last-sync timestamp in NVS
     (key names ≤ 15 chars — NVS hard limit), verify the write by reading it
     back, and on mismatch wipe the namespace and retry once.
     The sync time is mirrored in RTC memory as a tamper-proof reference.
7. **Read battery level/voltage.**
8. **Draw the display** — a single full-quality e-ink refresh showing:
   date (top left), battery % (top right), large hh:mm (center),
   battery voltage mV (bottom), plus a status line (`WiFi failed`,
   `NTP failed`, `Storage error`) whenever the last sync attempt failed.
9. **Compute sleep duration** — seconds until the next minute boundary plus a
   2 s offset, so wake-up lands just after the minute rolls over and the timer
   alarm can never fire before deep sleep is entered (minimum 3 s).
10. **Sleep** — clear the RTC IRQ state, turn off the LED, flush serial, and
    enter deep sleep via `M5.Power.timerSleep()`. The BM8563 timer interrupt
    wakes the ESP32 when the next minute is due.

## Display refresh strategy

`FULL_REFRESH_EVERY_MINUTES = 1` uses the full quality waveform on every
update, which keeps ghosting away entirely at the cost of the normal e-ink
flash each minute. Raise the value to update faster with only periodic full
refreshes (mild ghosting in between).

## Timekeeping notes

- The RTC stores local wall-clock time. After a DST changeover the display may
  be off by one hour until the next scheduled NTP sync corrects it (≤ 12 h).
- DST transitions themselves require no code change: they are encoded in the
  `TZ_INFO` POSIX string (`config.h`) and handled automatically during sync
  and verification.

## Configuration (`config.h`)

| Setting | Purpose |
|---|---|
| `WIFI_SSID` / `WIFI_PASSWORD` | Your network |
| `NTP_SERVER1..3` | Primary NTP servers |
| `TZ_INFO` | POSIX timezone string (incl. DST rules) |
| `NTP_RESYNC_INTERVAL_HOURS` | Hours between scheduled NTP syncs |
| `SYNC_RETRY_MINUTES` | Back-off between failed sync attempts |
| `FULL_REFRESH_EVERY_MINUTES` | Full e-ink refresh cadence |

## Diagnostics

Every boot prints its progress to serial (115200): wake cause, NVS state vs
the post-wake mirror, DNS resolution results, which NTP server answered,
RTC set/verify results, and NVS write sizes. The on-screen status line mirrors
persistent failures, so a wall-mounted unit shows what is wrong without a
serial monitor.
