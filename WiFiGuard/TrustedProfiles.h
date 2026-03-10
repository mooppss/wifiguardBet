#ifndef TRUSTED_PROFILES_H
#define TRUSTED_PROFILES_H

#include "Config.h"
#include "Types.h"

// Optional trusted-network profiles (no password storage in MVP).
// Stored only if user explicitly opts in.

#define MAX_TRUSTED_PROFILES 5

struct TrustedProfile {
  char ssid[33];
  uint8_t bssid[6];          // optional fingerprint (0s if unknown)
  char portalDomain[48];     // last seen portal domain (optional)
  uint8_t portalSafetyMin;   // expected minimum safety (0=SAFE, 1=CAUTION, 2=SUSPICIOUS) heuristic
};

class TrustedProfiles {
public:
  void begin();
  bool saveProfile(const TrustedProfile& p);
  bool isTrustedSsid(const char* ssid) const;
  void clearAll();
};

extern TrustedProfiles trustedProfiles;

#endif

