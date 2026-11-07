#include "adsb_client.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>

#include <cmath>

#include "adsb_parser.h"
#include "config.h"

namespace net {
namespace {

constexpr uint32_t kMinIntervalMs = 2000;
constexpr uint32_t kRateLimitedBackoffMs = 60000;
constexpr uint16_t kFailoverAfter = 3;
constexpr size_t kMaxBody = 512 * 1024;

class Lock {
 public:
  explicit Lock(SemaphoreHandle_t m) : m_(m) { xSemaphoreTake(m_, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(m_); }

 private:
  SemaphoreHandle_t m_;
};

}  // namespace

const char* AdsbClient::sourceName(uint8_t source) { return source == 1 ? "adsb.lol" : "adsb.fi"; }

void AdsbClient::begin() {
  if (!mutex_) mutex_ = xSemaphoreCreateMutex();
  if (task_) return;
  if (xTaskCreatePinnedToCore(taskEntry, "adsb", 12 * 1024, this, 1, &task_, 0) != pdPASS) {
    task_ = nullptr;
    log_e("cannot start the download task");
  }
}

void AdsbClient::configure(radar::LatLon center, uint16_t radiusNm, uint16_t intervalS,
                           uint8_t source) {
  Lock l(mutex_);
  if (radiusNm > 250) radiusNm = 250;
  if (radiusNm != radiusNm_ || source != preferred_ ||
      std::fabs(center.lat - center_.lat) > 1e-4 || std::fabs(center.lon - center_.lon) > 1e-4) {
    urgent_ = true;
  }
  center_ = center;
  radiusNm_ = radiusNm;
  intervalS_ = intervalS;
  preferred_ = source;
}

bool AdsbClient::takeLatest(std::vector<radar::Aircraft>& out, uint32_t& receivedMs) {
  Lock l(mutex_);
  if (!hasLatest_) return false;
  out.swap(latest_);
  latest_.clear();
  hasLatest_ = false;
  receivedMs = latestMs_;
  return true;
}

FeedStatus AdsbClient::status() {
  Lock l(mutex_);
  return status_;
}

void AdsbClient::taskEntry(void* self) { static_cast<AdsbClient*>(self)->run(); }

bool AdsbClient::fetchOnce(uint8_t source, std::vector<radar::Aircraft>& out, FeedStatus& st) {
  radar::LatLon c;
  uint16_t radius;
  {
    Lock l(mutex_);
    c = center_;
    radius = radiusNm_;
  }
  char url[160];
  if (source == 1) {
    snprintf(url, sizeof(url), "https://api.adsb.lol/v2/point/%.4f/%.4f/%u", c.lat, c.lon, radius);
  } else {
    snprintf(url, sizeof(url), "https://opendata.adsb.fi/api/v3/lat/%.4f/lon/%.4f/dist/%u", c.lat,
             c.lon, radius);
  }

  // One TLS client per source, kept alive across requests to avoid a full
  // handshake every few seconds.
  static NetworkClientSecure tls[2];
  static HTTPClient http[2];
  static bool init[2] = {false, false};
  const int i = source == 1 ? 1 : 0;
  if (!init[i]) {
    tls[i].useBuiltinCACertBundle();
    tls[i].setTimeout(12);
    http[i].setReuse(true);
    http[i].setTimeout(12000);
    http[i].setConnectTimeout(8000);
    http[i].setUserAgent("KonaRadar/" RADAR_FW_VERSION " (ESP32-S3 desk radar)");
    init[i] = true;
  }

  st.httpCode = 0;
  if (!http[i].begin(tls[i], url)) {
    snprintf(st.error, sizeof(st.error), "bad URL");
    return false;
  }
  const int code = http[i].GET();
  st.httpCode = code;
  if (code != HTTP_CODE_OK) {
    if (code < 0) {
      snprintf(st.error, sizeof(st.error), "%s", HTTPClient::errorToString(code).c_str());
    } else {
      snprintf(st.error, sizeof(st.error), "HTTP %d", code);
    }
    http[i].end();
    return false;
  }
  const int size = http[i].getSize();
  if (size > static_cast<int>(kMaxBody)) {
    snprintf(st.error, sizeof(st.error), "response too large");
    http[i].end();
    return false;
  }
  String body = http[i].getString();
  http[i].end();

  radar::ParseResult r = radar::parseAdsbJson(body.c_str(), body.length(), out);
  if (!r.ok) {
    snprintf(st.error, sizeof(st.error), "%s", r.error);
    return false;
  }
  log_d("%s: %u aircraft (%u listed), %u bytes", sourceName(source), (unsigned)r.accepted,
        (unsigned)r.listed, (unsigned)body.length());
  return true;
}

void AdsbClient::run() {
  std::vector<radar::Aircraft> list;
  uint8_t active = 0;
  uint32_t backoffMs = 0;
  uint32_t lastTry = 0;
  uint32_t okOnFallback = 0;
  uint8_t lastPreferred = 255;
  bool first = true;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));
    if (WiFi.status() != WL_CONNECTED) continue;

    uint32_t intervalMs;
    bool urgent;
    uint8_t preferred;
    {
      Lock l(mutex_);
      intervalMs = std::max<uint32_t>(kMinIntervalMs, intervalS_ * 1000UL);
      urgent = urgent_;
      preferred = preferred_;
    }
    if (preferred != lastPreferred) {
      active = lastPreferred = preferred;
      okOnFallback = 0;
    }
    const uint32_t now = millis();
    const uint32_t wait = backoffMs ? backoffMs : intervalMs;
    const bool due = first || now - lastTry >= wait || (urgent && now - lastTry >= kMinIntervalMs);
    if (!due) continue;
    first = false;
    lastTry = now;
    {
      Lock l(mutex_);
      urgent_ = false;
    }

    FeedStatus st = status();
    st.lastTryMs = now;
    const bool ok = fetchOnce(active, list, st);
    st.source = active;
    st.lastOk = ok;
    if (ok) {
      st.everOk = true;
      st.lastOkMs = millis();
      st.consecutiveErrors = 0;
      st.error[0] = 0;
      backoffMs = 0;
      // Return to the preferred source after a while on the fallback.
      if (active != preferred && ++okOnFallback >= 60) {
        active = preferred;
        okOnFallback = 0;
      }
    } else {
      ++st.consecutiveErrors;
      log_w("%s fetch failed: %s", sourceName(active), st.error);
      if (st.httpCode == 429) {
        backoffMs = kRateLimitedBackoffMs;
      } else {
        backoffMs = std::min<uint32_t>(60000, intervalMs << std::min<uint16_t>(st.consecutiveErrors, 4));
      }
      if (st.consecutiveErrors % kFailoverAfter == 0) active = active == 0 ? 1 : 0;
    }
    Lock l(mutex_);
    status_ = st;
    if (ok) {
      latest_.swap(list);
      hasLatest_ = true;
      latestMs_ = st.lastOkMs;
    }
  }
}

}  // namespace net
