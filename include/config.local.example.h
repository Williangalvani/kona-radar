// Copy to include/config.local.h (gitignored) to bake personal defaults into
// your own builds without committing them. Everything here can also be set
// later from the phone (setup portal or http://kona-radar.local/).
#pragma once

// Where the radar is mounted (decimal degrees). Replace the placeholders.
#define RADAR_OBSERVER_LAT 19.70000   // placeholder
#define RADAR_OBSERVER_LON -156.00000 // placeholder
#define RADAR_OBSERVER_ALT_FT 100

// Direction you face, degrees true. Leave at -1 to face the airport
// automatically, or set the exact bearing you measured.
#define RADAR_VIEW_BEARING_DEG (-1)
