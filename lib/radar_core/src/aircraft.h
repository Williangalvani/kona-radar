// One aircraft as reported by an ADS-B aggregator (readsb / ADSBexchange v2
// JSON format, used by adsb.fi and adsb.lol).
#pragma once

#include <cstdint>

namespace radar {

struct Aircraft {
  char hex[8] = {};        // ICAO 24-bit address, lowercase hex ("a5d814")
  char callsign[10] = {};  // trimmed, may be empty
  char reg[12] = {};       // registration ("N476HA"), may be empty
  char type[6] = {};       // ICAO type designator ("B712"), may be empty
  char desc[48] = {};      // type description ("BOEING 717-200"), adsb.fi only
  char owner[40] = {};     // registered owner/operator, adsb.fi only
  char squawk[6] = {};
  char category[4] = {};   // emitter category ("A3"), may be empty

  double lat = 0;
  double lon = 0;

  int32_t altFt = 0;        // barometric altitude (geometric if baro missing)
  int32_t vertRateFpm = 0;
  float gsKt = 0;           // ground speed
  float trackDeg = 0;       // true track over ground
  float seenPosS = 0;       // age of the position at the server, seconds

  bool hasAlt = false;
  bool onGround = false;
  bool hasGs = false;
  bool hasTrack = false;
  bool hasVertRate = false;
  bool emergency = false;   // emergency status set or squawk 7500/7600/7700
};

}  // namespace radar
