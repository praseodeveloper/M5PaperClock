/*
   M5Paper Clock - Utility Functions
   RSSI signal quality helpers.
*/
#pragma once

/* Convert RSSI to quality percentage (0-100) */
int WifiGetRssiAsQualityInt(int rssi) {
  int quality = 0;

  if (rssi <= -100) {
    quality = 0;
  } else if (rssi >= -50) {
    quality = 100;
  } else {
    quality = 2 * (rssi + 100);
  }
  return quality;
}

/* Convert RSSI to quality string */
String WifiGetRssiAsQuality(int rssi) {
  return String(WifiGetRssiAsQualityInt(rssi));
}
