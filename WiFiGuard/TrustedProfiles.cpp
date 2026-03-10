#include "TrustedProfiles.h"
#include "Settings.h"
#include <Preferences.h>
#include <string.h>

TrustedProfiles trustedProfiles;

static Preferences prefsTrust;
static TrustedProfile profiles_[MAX_TRUSTED_PROFILES];
static uint8_t profileCount_ = 0;

static void safeCopy(char* dst, int dstSize, const char* src) {
  if (!dst || dstSize <= 0) return;
  if (!src) { dst[0] = '\0'; return; }
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

void TrustedProfiles::begin() {
  profileCount_ = 0;
  memset(profiles_, 0, sizeof(profiles_));
  if (settings.get().sessionOnly) return;
  if (!prefsTrust.begin("wguard_trust", true)) return;
  profileCount_ = prefsTrust.getUChar("cnt", 0);
  if (profileCount_ > MAX_TRUSTED_PROFILES) profileCount_ = MAX_TRUSTED_PROFILES;
  prefsTrust.getBytes("profiles", profiles_, sizeof(profiles_));
  prefsTrust.end();
}

bool TrustedProfiles::isTrustedSsid(const char* ssid) const {
  if (!ssid || !ssid[0]) return false;
  for (uint8_t i = 0; i < profileCount_; i++) {
    if (profiles_[i].ssid[0] && strcmp(profiles_[i].ssid, ssid) == 0) return true;
  }
  return false;
}

bool TrustedProfiles::saveProfile(const TrustedProfile& p) {
  if (settings.get().sessionOnly) return false;
  if (!p.ssid[0]) return false;

  // Update existing
  for (uint8_t i = 0; i < profileCount_; i++) {
    if (strcmp(profiles_[i].ssid, p.ssid) == 0) {
      profiles_[i] = p;
      if (!prefsTrust.begin("wguard_trust", false)) return false;
      prefsTrust.putUChar("cnt", profileCount_);
      prefsTrust.putBytes("profiles", profiles_, sizeof(profiles_));
      prefsTrust.end();
      return true;
    }
  }

  // Append (or replace oldest slot 0 if full)
  uint8_t idx = profileCount_ < MAX_TRUSTED_PROFILES ? profileCount_ : 0;
  profiles_[idx] = p;
  if (profileCount_ < MAX_TRUSTED_PROFILES) profileCount_++;

  if (!prefsTrust.begin("wguard_trust", false)) return false;
  prefsTrust.putUChar("cnt", profileCount_);
  prefsTrust.putBytes("profiles", profiles_, sizeof(profiles_));
  prefsTrust.end();
  return true;
}

void TrustedProfiles::clearAll() {
  profileCount_ = 0;
  memset(profiles_, 0, sizeof(profiles_));
  if (!prefsTrust.begin("wguard_trust", false)) return;
  prefsTrust.putUChar("cnt", 0);
  prefsTrust.remove("profiles");
  prefsTrust.end();
}

