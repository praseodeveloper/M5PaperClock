/*
   M5Paper Clock - WiFi Functions
   Functions for WiFi connection.
*/
#pragma once
#include <WiFi.h>
#include "config.h"

/* Start WiFi connection, return true if successful */
bool StartWiFi(int &rssi) {
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

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
    Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("\nWiFi FAILED");
    return false;
  }
}

/* Stop WiFi connection */
void StopWiFi() {
  Serial.println("WiFi stopped");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
