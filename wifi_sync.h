/*
   M5Paper Clock - WiFi Functions
   Functions for WiFi connection.
*/
#pragma once
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_idf_version.h>
#include "config.h"

/* Start WiFi connection, return true if successful */
bool StartWiFi(int &rssi) {
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  // Do not write credentials to NVS on every boot (avoids flash wear)
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  for (int retry = 0; WiFi.status() != WL_CONNECTED && retry < 30; retry++) {
    delay(500);
    Serial.print(".");
  }

  rssi = 0;
  if (WiFi.status() == WL_CONNECTED) {
    rssi = WiFi.RSSI();
    Serial.print("\nWiFi connected, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nWiFi FAILED");
    return false;
  }
}

/* Stop WiFi connection */
void StopWiFi() {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_sntp_stop();
#else
  sntp_stop();
#endif
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi stopped");
}
