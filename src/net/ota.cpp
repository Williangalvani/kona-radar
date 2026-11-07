#include "ota.h"

#include <ArduinoOTA.h>
#include <Update.h>
#include <esp_app_desc.h>
#include <esp_image_format.h>
#include <esp_ota_ops.h>

#include "app/settings_store.h"
#include "config.h"

// Keep the new image "pending verify" until Ota::loop() has seen it work;
// the Arduino core would otherwise confirm it before setup() runs.
extern "C" bool verifyRollbackLater() { return true; }

namespace net {
namespace {

constexpr uint32_t kConfirmAfterBootMs = 20000;
constexpr uint32_t kConfirmAfterNetMs = 5000;

}  // namespace

void Ota::begin(app::SettingsStore* store, ProgressFn progress) {
  store_ = store;
  progress_ = std::move(progress);
  password_ = store_ ? store_->otaPassword() : String();

  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  pendingVerify_ = running && esp_ota_get_state_partition(running, &state) == ESP_OK &&
                   state == ESP_OTA_IMG_PENDING_VERIFY;
  log_i("firmware " RADAR_FW_VERSION " from %s%s", runningPartition(),
        pendingVerify_ ? " (new, waiting for a healthy start)" : "");
}

const char* Ota::runningPartition() const {
  const esp_partition_t* running = esp_ota_get_running_partition();
  return running ? running->label : "?";
}

void Ota::loop(bool online, bool setupActive) {
  const uint32_t now = millis();
  if (rebootAtMs_ && static_cast<int32_t>(now - rebootAtMs_) >= 0) {
    log_i("rebooting into the new firmware");
    delay(50);
    ESP.restart();
  }

  if (pendingVerify_) {
    if (online || setupActive) {
      if (!onlineSinceMs_) onlineSinceMs_ = now ? now : 1;
      if (now > kConfirmAfterBootMs && now - onlineSinceMs_ > kConfirmAfterNetMs) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) log_i("firmware " RADAR_FW_VERSION " confirmed");
        pendingVerify_ = false;
      }
    } else {
      onlineSinceMs_ = 0;
    }
  }

  if (!arduinoOtaStarted_ && online && password_.length()) {
    // mDNS belongs to WebUi (it also advertises the OTA port).
    ArduinoOTA.setHostname(RADAR_HOSTNAME);
    ArduinoOTA.setPassword(password_.c_str());
    ArduinoOTA.setMdnsEnabled(false);
    ArduinoOTA.onStart([this] {
      busy_ = true;
      log_i("espota: update started");
      report(0);
    });
    ArduinoOTA.onProgress([this](unsigned int done, unsigned int total) {
      if (total) report(static_cast<int>(static_cast<uint64_t>(done) * 100 / total));
    });
    ArduinoOTA.onEnd([this] {
      log_i("espota: update complete");
      report(100);
    });
    ArduinoOTA.onError([this](ota_error_t e) {
      log_w("espota: update failed (%u)", static_cast<unsigned>(e));
      busy_ = false;
      report(-1);
    });
    ArduinoOTA.begin();
    arduinoOtaStarted_ = true;
  }
  if (arduinoOtaStarted_) ArduinoOTA.handle();
}

bool Ota::checkPassword(const String& candidate) const {
  if (password_.length() == 0 || candidate.length() != password_.length()) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < password_.length(); ++i) diff |= static_cast<uint8_t>(candidate[i] ^ password_[i]);
  return diff == 0;
}

bool Ota::setPassword(const String& pass, String& error) {
  if (pass.length() < 8 || pass.length() > 32) {
    error = "The password must be 8 to 32 characters.";
    return false;
  }
  for (size_t i = 0; i < pass.length(); ++i) {
    if (pass[i] <= ' ' || pass[i] > '~') {
      error = "Use letters, digits and punctuation only (no spaces).";
      return false;
    }
  }
  if (busy_) {
    error = "An update is running.";
    return false;
  }
  password_ = pass;
  if (store_) store_->setOtaPassword(pass);
  if (arduinoOtaStarted_) ArduinoOTA.setPassword(password_.c_str());
  log_i("update password changed");
  return true;
}

void Ota::report(int pct) {
  const uint32_t now = millis();
  if (pct == pct_ && pct >= 0) return;
  // Redrawing the screen costs upload time; a few times a second is plenty.
  if (pct > 0 && pct < 100 && now - lastReportMs_ < 250) return;
  pct_ = pct;
  lastReportMs_ = now;
  if (progress_) progress_(pct);
}

bool Ota::uploadStart(size_t size, String& error) {
  if (busy_) {
    error = "Another update is running.";
    return false;
  }
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (!next) {
    error = "This partition table has no second app slot.";
    return false;
  }
  if (size && size > next->size) {
    error = "That file is larger than the app slot.";
    return false;
  }
  if (!Update.begin(size ? size : UPDATE_SIZE_UNKNOWN, U_FLASH)) {
    error = String("Cannot start the update: ") + Update.errorString();
    return false;
  }
  busy_ = true;
  total_ = size;
  written_ = 0;
  headerChecked_ = false;
  log_i("web update: receiving %u bytes into %s", static_cast<unsigned>(size), next->label);
  report(0);
  return true;
}

bool Ota::uploadWrite(const uint8_t* data, size_t len, String& error) {
  if (!busy_) return false;
  if (!headerChecked_) {
    headerChecked_ = true;
    const size_t descAt = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    const auto* h = reinterpret_cast<const esp_image_header_t*>(data);
    if (len < descAt + sizeof(esp_app_desc_t) || h->magic != ESP_IMAGE_HEADER_MAGIC) {
      error = "That is not an ESP32 firmware file (use .pio/build/kona-radar/firmware.bin).";
      uploadAbort();
      return false;
    }
    if (h->chip_id != CONFIG_IDF_FIRMWARE_CHIP_ID) {
      error = "That firmware is built for a different ESP32 chip.";
      uploadAbort();
      return false;
    }
    const auto* d = reinterpret_cast<const esp_app_desc_t*>(data + descAt);
    if (d->magic_word != ESP_APP_DESC_MAGIC_WORD) {
      // A merged/factory image starts with the bootloader, not an app.
      error = "That is not an app image (use firmware.bin, not a merged or bootloader file).";
      uploadAbort();
      return false;
    }
    log_i("web update: %.32s %.32s built %.16s %.16s", d->project_name, d->version, d->date, d->time);
  }
  if (Update.write(const_cast<uint8_t*>(data), len) != len) {
    error = String("Write failed: ") + Update.errorString();
    uploadAbort();
    return false;
  }
  written_ += len;
  if (total_) report(static_cast<int>(static_cast<uint64_t>(written_) * 99 / total_));
  return true;
}

bool Ota::uploadEnd(String& error) {
  if (!busy_) return false;
  // end() checks the image (and its SHA-256) before switching the boot slot.
  if (!Update.end(true)) {
    error = String("The firmware did not verify: ") + Update.errorString();
    busy_ = false;
    report(-1);
    return false;
  }
  log_i("web update: %u bytes written and verified", static_cast<unsigned>(written_));
  report(100);
  return true;
}

void Ota::uploadAbort() {
  if (!busy_) return;
  Update.abort();
  busy_ = false;
  log_w("web update aborted after %u bytes", static_cast<unsigned>(written_));
  report(-1);
}

void Ota::rebootSoon(uint32_t delayMs) { rebootAtMs_ = millis() + delayMs; }

}  // namespace net
