#include "places.h"

namespace radar {
namespace {

// FAA airport diagram threshold coordinates.
const Runway kKoaRunways[] = {
    {{"17", "35"}, {{19.75385525, -156.04428864}, {19.72367472, -156.04697358}}},
};

}  // namespace

const Place kAirports[] = {
    {"KOA", "Kona International", {19.738765, -156.045631}, kKoaRunways, 1},
    {"ITO", "Hilo International", {19.7214, -155.0485}},
    {"MUE", "Waimea-Kohala", {20.0013, -155.6681}},
    {"UPP", "Upolu", {20.2653, -155.8600}},
    {"OGG", "Kahului", {20.8986, -156.4305}},
    {"JHM", "Kapalua", {20.9629, -156.6730}},
    {"LNY", "Lanai", {20.7856, -156.9514}},
    {"MKK", "Molokai", {21.1529, -157.0963}},
    {"HNL", "Honolulu", {21.3187, -157.9224}},
    {"LIH", "Lihue", {21.9760, -159.3390}},
};

const size_t kAirportCount = sizeof(kAirports) / sizeof(kAirports[0]);

const Place* airportNear(const LatLon& pos) {
  const Place* best = nullptr;
  double bestNm = 3.0;
  for (size_t i = 0; i < kAirportCount; ++i) {
    const double d = distanceNm(pos, kAirports[i].pos);
    if (d < bestNm) {
      bestNm = d;
      best = &kAirports[i];
    }
  }
  return best;
}

}  // namespace radar
