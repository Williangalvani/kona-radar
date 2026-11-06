#include "format.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "geo.h"

namespace radar {
namespace {

struct CodeName {
  const char* code;
  const char* name;
};

// Common types around Hawaii (used when the feed has no description).
const CodeName kTypes[] = {
    {"A20N", "Airbus A320neo"},     {"A21N", "Airbus A321neo"},   {"A320", "Airbus A320"},
    {"A321", "Airbus A321"},        {"A332", "Airbus A330-200"},  {"A333", "Airbus A330-300"},
    {"A339", "Airbus A330-900neo"}, {"A359", "Airbus A350-900"},  {"A388", "Airbus A380"},
    {"B712", "Boeing 717"},         {"B737", "Boeing 737-700"},   {"B738", "Boeing 737-800"},
    {"B739", "Boeing 737-900"},     {"B37M", "Boeing 737 MAX 7"}, {"B38M", "Boeing 737 MAX 8"},
    {"B39M", "Boeing 737 MAX 9"},   {"B752", "Boeing 757-200"},   {"B753", "Boeing 757-300"},
    {"B763", "Boeing 767-300"},     {"B764", "Boeing 767-400"},   {"B772", "Boeing 777-200"},
    {"B77W", "Boeing 777-300ER"},   {"B788", "Boeing 787-8"},     {"B789", "Boeing 787-9"},
    {"B78X", "Boeing 787-10"},      {"B744", "Boeing 747-400"},   {"B748", "Boeing 747-8"},
    {"C17", "Boeing C-17"},         {"C130", "Lockheed C-130"},   {"C30J", "Lockheed C-130J"},
    {"K35R", "Boeing KC-135"},      {"P8", "Boeing P-8"},         {"C208", "Cessna Caravan"},
    {"C172", "Cessna 172"},         {"C182", "Cessna 182"},       {"C206", "Cessna 206"},
    {"C72R", "Cessna 172RG"},       {"PC12", "Pilatus PC-12"},    {"BE20", "Beech King Air"},
    {"B350", "Beech King Air 350"}, {"DHC6", "Twin Otter"},       {"C408", "Cessna SkyCourier"},
    {"R22", "Robinson R22"},        {"R44", "Robinson R44"},      {"R66", "Robinson R66"},
    {"AS50", "Airbus H125"},        {"EC30", "Airbus H130"},      {"EC35", "Airbus H135"},
    {"AS65", "MH-65 Dolphin"},      {"H60", "Black Hawk"},        {"GLF5", "Gulfstream G550"},
    {"GLF6", "Gulfstream G650"},    {"GL5T", "Bombardier Global 5000"},
    {"GLEX", "Bombardier Global Express"},
    {"CL60", "Bombardier Challenger"},
};

// ICAO airline designators seen around Hawaii.
const CodeName kAirlines[] = {
    {"HAL", "Hawaiian"},            {"ASA", "Alaska"},           {"SWA", "Southwest"},
    {"UAL", "United"},              {"AAL", "American"},         {"DAL", "Delta"},
    {"WJA", "WestJet"},             {"ACA", "Air Canada"},       {"JAL", "Japan Airlines"},
    {"ANA", "ANA"},  {"KAL", "Korean Air"},       {"QFA", "Qantas"},
    {"ANZ", "Air New Zealand"},     {"FJI", "Fiji Airways"},     {"CPA", "Cathay Pacific"},
    {"FDX", "FedEx"},               {"UPS", "UPS"},              {"MUL", "Mokulele"},
    {"KMK", "Kamaka Air"},          {"SCX", "Sun Country"},      {"AAH", "Aloha Air Cargo"},
    {"TCX", "Transair"},            {"VJA", "Vista America"},    {"EJA", "NetJets"},
    {"RCH", "US Air Force (Reach)"}, {"CPT", "Corporate Air"},   {"PCO", "Pacific Coastal"},
    {"JZA", "Jazz"},                {"ZLX", "Zela Air"},         {"STT", "Southern Airways"},
};

void copyStr(char* buf, size_t cap, const char* s) {
  if (cap == 0) return;
  std::snprintf(buf, cap, "%s", s ? s : "");
}

}  // namespace

const char* displayName(const Aircraft& a, char* buf, size_t cap) {
  if (a.callsign[0]) {
    copyStr(buf, cap, a.callsign);
  } else if (a.reg[0]) {
    copyStr(buf, cap, a.reg);
  } else {
    copyStr(buf, cap, a.hex);
    for (char* p = buf; *p; ++p) *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
  }
  return buf;
}

const char* withThousands(long v, char* buf, size_t cap) {
  char digits[24];
  std::snprintf(digits, sizeof(digits), "%ld", std::labs(v));
  const size_t n = std::strlen(digits);
  char out[32];
  size_t o = 0;
  if (v < 0) out[o++] = '-';
  for (size_t i = 0; i < n; ++i) {
    out[o++] = digits[i];
    const size_t left = n - i - 1;
    if (left > 0 && left % 3 == 0) out[o++] = ',';
  }
  out[o] = 0;
  copyStr(buf, cap, out);
  return buf;
}

const char* formatAltitude(const Aircraft& a, char* buf, size_t cap) {
  if (a.onGround) {
    copyStr(buf, cap, "GND");
  } else if (!a.hasAlt) {
    copyStr(buf, cap, "--");
  } else {
    const long rounded = std::lround(a.altFt / 100.0) * 100;
    char num[24];
    withThousands(rounded, num, sizeof(num));
    std::snprintf(buf, cap, "%s ft", num);
  }
  return buf;
}

const char* formatSpeed(const Aircraft& a, char* buf, size_t cap) {
  if (!a.hasGs) {
    copyStr(buf, cap, "--");
  } else {
    std::snprintf(buf, cap, "%ld kt", std::lround(a.gsKt));
  }
  return buf;
}

int verticalTrend(const Aircraft& a) {
  if (!a.hasVertRate || a.onGround) return 0;
  if (a.vertRateFpm >= 300) return 1;
  if (a.vertRateFpm <= -300) return -1;
  return 0;
}

const char* compassPoint(double deg) {
  static const char* kNames[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  const int idx = static_cast<int>(std::floor(wrap360(deg + 22.5) / 45.0)) % 8;
  return kNames[idx];
}

const char* titleCase(const char* in, char* buf, size_t cap) {
  if (cap == 0) return buf;
  size_t o = 0;
  const char* p = in ? in : "";
  while (*p && o + 1 < cap) {
    if (*p == ' ') {
      buf[o++] = *p++;
      continue;
    }
    const char* end = p;
    bool hasDigit = false, hasLower = false, roman = true;
    while (*end && *end != ' ') {
      const char c = *end;
      hasDigit |= std::isdigit(static_cast<unsigned char>(c)) != 0;
      hasLower |= std::islower(static_cast<unsigned char>(c)) != 0;
      roman &= c == 'I' || c == 'V' || c == 'X';
      ++end;
    }
    const bool keep = hasDigit || hasLower || roman;
    for (const char* q = p; q < end && o + 1 < cap; ++q) {
      const unsigned char c = static_cast<unsigned char>(*q);
      buf[o++] = keep ? *q : static_cast<char>(q == p ? std::toupper(c) : std::tolower(c));
    }
    p = end;
  }
  buf[o] = 0;
  return buf;
}

const char* aircraftTypeName(const Aircraft& a, char* buf, size_t cap) {
  if (a.desc[0]) return titleCase(a.desc, buf, cap);
  for (const CodeName& t : kTypes) {
    if (std::strcmp(t.code, a.type) == 0) {
      copyStr(buf, cap, t.name);
      return buf;
    }
  }
  copyStr(buf, cap, a.type);
  return buf;
}

const char* operatorName(const Aircraft& a, char* buf, size_t cap) {
  // Airline callsigns are three letters followed by a flight number.
  const char* cs = a.callsign;
  if (std::strlen(cs) >= 4 && std::isalpha(static_cast<unsigned char>(cs[0])) &&
      std::isalpha(static_cast<unsigned char>(cs[1])) &&
      std::isalpha(static_cast<unsigned char>(cs[2])) &&
      std::isdigit(static_cast<unsigned char>(cs[3]))) {
    for (const CodeName& al : kAirlines) {
      if (std::strncmp(al.code, cs, 3) == 0) {
        copyStr(buf, cap, al.name);
        return buf;
      }
    }
  }
  return titleCase(a.owner, buf, cap);
}

const char* relativeDirection(double relDeg, char* buf, size_t cap) {
  if (std::isnan(relDeg)) {
    copyStr(buf, cap, "");
  } else {
    const long a = std::lround(std::fabs(relDeg));
    if (a <= 7)
      copyStr(buf, cap, "AHEAD");
    else if (a >= 165)
      copyStr(buf, cap, "BEHIND");
    else
      std::snprintf(buf, cap, "%s %ld\u00b0", relDeg > 0 ? "RIGHT" : "LEFT", a);
  }
  return buf;
}

const char* lookCue(double relDeg, double bearingDeg, char* buf, size_t cap) {
  if (std::isnan(relDeg)) {
    std::snprintf(buf, cap, "LOOK %s", compassPoint(bearingDeg));
  } else {
    const double a = std::fabs(relDeg);
    std::snprintf(buf, cap, "LOOK %s",
                  a <= 7 ? "AHEAD" : a >= 165 ? "BEHIND" : relDeg > 0 ? "RIGHT" : "LEFT");
  }
  return buf;
}

const char* etaText(float etaS, char* buf, size_t cap) {
  if (etaS < 20) {
    copyStr(buf, cap, "NOW");
  } else {
    const long m = static_cast<long>(std::ceil(etaS / 60.0f));
    std::snprintf(buf, cap, "~%ld MIN", m < 1 ? 1 : m);
  }
  return buf;
}

const char* bearingText(double bearingDeg, char* buf, size_t cap) {
  const long b = std::lround(wrap360(bearingDeg)) % 360;
  std::snprintf(buf, cap, "%s %03ld\u00b0T", compassPoint(bearingDeg), b);
  return buf;
}

}  // namespace radar
