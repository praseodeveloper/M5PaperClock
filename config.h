/*
   M5Paper Clock - Configuration
   Change these values to match your network and timezone.
*/
#pragma once

#define WIFI_SSID "<SSID>"
#define WIFI_PASSWORD "<PASSWORD>"

// NTP servers - global anycast services with very high availability
#define NTP_SERVER1 "time.cloudflare.com"
#define NTP_SERVER2 "time.google.com"
#define NTP_SERVER3 "pool.ntp.org"

// Timezone string (https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv)
// CET Central European Time (Germany, etc.)
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// Re-synchronize with NTP every N hours to compensate for RTC drift
#define NTP_RESYNC_INTERVAL_HOURS 12

// When a resync is due but keeps failing (router down etc.), retry only
// every N minutes instead of on every wake. Time keeps running off the RTC.
#define SYNC_RETRY_MINUTES 15

// Full e-ink refresh every N minutes; prevents ghosting from partial
// refreshes. 1 = full refresh on every update (slow flash each minute).
// Raise to e.g. 5 for faster, flicker-lighter updates with mild ghosting.
#define FULL_REFRESH_EVERY_MINUTES 1
