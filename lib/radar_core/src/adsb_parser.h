// Parser for readsb / ADSBexchange-v2 style JSON ("ac" or "aircraft" array),
// as served by adsb.fi (/api/v3/lat/../lon/../dist/..) and adsb.lol
// (/v2/point/../../..).
#pragma once

#include <cstddef>
#include <vector>

#include "aircraft.h"

namespace radar {

struct ParseResult {
  bool ok = false;
  char error[48] = {};      // human-readable reason when !ok
  size_t listed = 0;        // entries in the JSON array
  size_t accepted = 0;      // entries with a usable position
  double serverTimeMs = 0;  // "now" field (normalized to milliseconds), 0 if absent
};

// Parses `len` bytes of `json` and replaces the contents of `out`. Entries
// without lat/lon are skipped. At most `maxAircraft` are kept.
ParseResult parseAdsbJson(const char* json, size_t len, std::vector<Aircraft>& out,
                          size_t maxAircraft = 300);

}  // namespace radar
