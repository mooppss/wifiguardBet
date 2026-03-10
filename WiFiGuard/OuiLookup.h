#ifndef OUILOOKUP_H
#define OUILOOKUP_H

#include "Config.h"
#include "Types.h"

// Look up vendor name from BSSID OUI (first 3 bytes). Fills net.vendor (max 15 chars).
void ouiLookup(NetworkRecord& net);

#endif
