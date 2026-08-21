/*
   M5Paper Clock - Configuration
   Change these values to match your network and timezone.
*/
#pragma once

#define WIFI_SSID "<SSID>"
#define WIFI_PASSWORD "<PASSWORD>"

// NTP servers
#define NTP_SERVER1 "ptbtime1.ptb.de"
#define NTP_SERVER2 "ptbtime2.ptb.de"
#define NTP_SERVER3 "de.pool.ntp.org"

// Timezone string (https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv)
// CET Central European Time (Germany, etc.)
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
