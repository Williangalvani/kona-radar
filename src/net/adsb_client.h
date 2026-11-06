// Background downloader for aircraft positions (FreeRTOS task on core 0).
//
// Sources (same readsb JSON schema, no API key):
//   0 adsb.fi   https://opendata.adsb.fi/api/v3/lat/{lat}/lon/{lon}/dist/{nm}
//   1 adsb.lol  https://api.adsb.lol/v2/point/{lat}/{lon}/{nm}
// Both allow ~1 request/s; we poll every few seconds, back off on errors and
// fail over to the other source after repeated failures.
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <vector>

#include "aircraft.h"
#include "geo.h"

namespace net {

struct FeedStatus {
  bool everOk = false;
  bool lastOk = false;
  uint8_t source = 0;
  uint32_t lastOkMs = 0;      // millis() of the last good download
  uint32_t lastTryMs = 0;
  uint16_t consecutiveErrors = 0;
  int httpCode = 0;
  char error[48] = {};
};

class AdsbClient {
 public:
  static const char* sourceName(uint8_t source);

  void begin();
  // Thread-safe; takes effect on the next request (and triggers one soon if
  // the radius or center changed).
  void configure(radar::LatLon center, uint16_t radiusNm, uint16_t intervalS, uint8_t source);
  // Moves the newest download (if any) into `out`. Call from the UI thread.
  bool takeLatest(std::vector<radar::Aircraft>& out, uint32_t& receivedMs);
  FeedStatus status();

 private:
  static void taskEntry(void* self);
  void run();
  bool fetchOnce(uint8_t source, std::vector<radar::Aircraft>& out, FeedStatus& st);

  SemaphoreHandle_t mutex_ = nullptr;
  TaskHandle_t task_ = nullptr;
  // Guarded by mutex_.
  radar::LatLon center_{0, 0};
  uint16_t radiusNm_ = 50;
  uint16_t intervalS_ = 5;
  uint8_t preferred_ = 0;
  bool urgent_ = false;
  std::vector<radar::Aircraft> latest_;
  bool hasLatest_ = false;
  uint32_t latestMs_ = 0;
  FeedStatus status_;
};

}  // namespace net
