#ifndef PROTECTED_JOIN_H
#define PROTECTED_JOIN_H

#include "Config.h"
#include "Types.h"

// Phone-Assisted Secure Join (defensive):
// - User enters credentials on phone via local setup hotspot + webpage
// - Credentials are RAM-only and cleared by default after verification
// - No guessing/bruteforce, no portal bypass, no credential logging/export

enum ProtectedJoinPhase {
  PJ_IDLE = 0,
  PJ_SETUP_AP,
  PJ_WAITING_FOR_PHONE,
  PJ_CONNECTING,
  PJ_CHECKING,
  PJ_RESULTS
};

struct ProtectedJoinStatus {
  ProtectedJoinPhase   phase;
  ProtectedJoinVerdict verdict;
  uint8_t              progressStep;     // 0..N (for UI)
  bool                 hasPortalUrl;
  char                 targetSsid[33];
  char                 portalDomain[48];
  PortalSafety         portalSafety;
  uint8_t              portalSafetyScore;
  int16_t              benchmarkAvgMs;
  int16_t              benchmarkJitterMs;
  uint8_t              benchmarkPings;
  bool                 connected;
  bool                 dhcpOk;
  bool                 dnsOk;
  bool                 httpOk;
};

class ProtectedJoin {
public:
  void begin();

  // Start the phone-assisted join flow for a specific SSID (SSID locked to selection in MVP).
  void start(const char* targetSsid);

  // Cancel flow; clears credentials and returns to browsing.
  void cancel();

  // Non-blocking update; call frequently while STATE_PROTECTED_JOIN is active.
  void update();

  bool isActive() const { return active_; }
  ProtectedJoinStatus getStatus() const { return status_; }

  // Values for device screen during AP setup
  const char* getSetupApSsid() const { return apSsid_; }
  const char* getSetupApPass() const { return apPass_; }
  const char* getPairingCode() const { return pairingCode_; }

  // User actions (two-button UX)
  bool isInResults() const { return active_ && status_.phase == PJ_RESULTS; }
  void retry();   // clears state and restarts AP setup for same SSID

private:
  void startSoftAp_();
  void stopSoftAp_();
  void startWebServer_();
  void stopWebServer_();
  void clearSensitive_();

  void beginConnect_();
  void updateConnect_();

  void beginChecks_();
  void updateChecks_();
  void finalizeVerdict_();

  void buildHtml_(char* out, int outSize) const;
  bool validatePairing_(const char* code) const;

  // Session state
  bool     active_;
  uint32_t phaseStartMs_;
  uint8_t  failureCount_;

  // Setup AP credentials (rotated per session)
  char apSsid_[24];
  char apPass_[16];
  char pairingCode_[8];

  // Sensitive: RAM-only password buffer
  char password_[65];  // WPA2/WPA3 passphrase max 63; +1 NUL
  bool hasPassword_;

  // Internal check phases
  uint8_t checkPhase_;
  uint32_t checkStartMs_;

  // Portal URL is kept only long enough to analyze; not exposed by default on secured join.
  char portalUrl_[96];

  ProtectedJoinStatus status_;
};

extern ProtectedJoin protectedJoin;

#endif

