/*
   M5Paper Clock - Sensor Functions
   Reads the internal SHT30 temperature / humidity sensor (SDA=21, SCL=22).
   Goes through M5.In_I2C, since M5Unified owns the internal I2C port.
*/
#pragma once
#include <M5Unified.h>
#include <cstdint>

static constexpr uint8_t kSht30Addr = 0x44;
static constexpr uint32_t kSht30FreqHz = 100000;

static uint8_t Sht30Crc8(const uint8_t* data, int len) {
  uint8_t crc = 0xFF;
  while (len-- > 0) {
    crc ^= *data++;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                         : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

static bool ReadTemperatureHumidity(float& outTempC, float& outHumidityPct) {
  if (!M5.In_I2C.isEnabled()) {
    return false;
  }

  // 0x2400 = single shot, high repeatability, clock stretching disabled
  if (!M5.In_I2C.writeRegister8(kSht30Addr, 0x24, 0x00, kSht30FreqHz)) {
    return false;
  }
  delay(20);

  uint8_t buf[6];
  if (!(M5.In_I2C.start(kSht30Addr, true, kSht30FreqHz) && M5.In_I2C.read(buf, sizeof(buf), true) && M5.In_I2C.stop())) {
    return false;
  }
  if (Sht30Crc8(buf, 2) != buf[2] || Sht30Crc8(buf + 3, 2) != buf[5]) {
    return false;
  }

  uint16_t rawTemp = static_cast<uint16_t>((buf[0] << 8) | buf[1]);
  uint16_t rawHum = static_cast<uint16_t>((buf[3] << 8) | buf[4]);
  outTempC = -45.0f + 175.0f * rawTemp / 65535.0f;
  outHumidityPct = 100.0f * rawHum / 65535.0f;
  return true;
}
