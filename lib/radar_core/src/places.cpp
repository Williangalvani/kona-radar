#include "places.h"

namespace radar {

const Place kAirports[] = {
    {"KOA", "Kona International", {19.7388, -156.0456}},
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

}  // namespace radar
