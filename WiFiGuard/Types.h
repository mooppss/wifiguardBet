#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include "Config.h"

// --- Auth / security ---
enum AuthType {
  AUTH_OPEN,
  AUTH_WEP,
  AUTH_WPA,
  AUTH_WPA2,
  AUTH_WPA3,
  AUTH_WPA_WPA2,
  AUTH_WPA2_WPA3,
  AUTH_UNKNOWN
};

// --- Connectivity grading ---
enum ConnectivityGrade {
  GRADE_FAST,
  GRADE_NORMAL,
  GRADE_SLOW,
  GRADE_PORTAL,
  GRADE_OFFLINE,
  GRADE_PROTECTED,
  GRADE_FAILED,
  GRADE_UNTESTED
};

enum PortalResult {
  PORTAL_NORMAL,
  PORTAL_REDIRECT_LOGIN,
  PORTAL_INTERCEPT,
  PORTAL_NO_INTERNET,
  PORTAL_FAILED,
  PORTAL_CREDENTIAL_REQUIRED,
  PORTAL_UNKNOWN
};

// --- Portal safety (captive portal analysis) ---
enum PortalSafety {
  PORTAL_SAFE,
  PORTAL_CAUTION,
  PORTAL_SUSPICIOUS
};

// --- Device state ---
enum DeviceState {
  STATE_IDLE,
  STATE_SCANNING,
  STATE_PROCESSING,
  STATE_BROWSING,
  STATE_TESTING,
  STATE_PROTECTED_JOIN,
  STATE_EXPORT,
  STATE_SETTINGS,
  STATE_SLEEP,
  STATE_STABILITY_MONITOR,
  STATE_DEBUG
};

// --- Protected network verification verdict (post-join, defensive) ---
enum ProtectedJoinVerdict {
  PJV_READY_TO_USE,
  PJV_LOGIN_REQUIRED,
  PJV_NO_INTERNET,
  PJV_SLOW_CONNECTION,
  PJV_CAUTION,
  PJV_SUSPICIOUS_BEHAVIOR,
  PJV_COULD_NOT_VERIFY
};

// --- Sort / filter ---
enum SortMode {
  SORT_RSSI,
  SORT_RISK,
  SORT_OPEN_FIRST,
  SORT_ENCRYPTED_FIRST,
  SORT_CHANNEL,
  SORT_NEWEST,
  SORT_PORTAL_FIRST,
  SORT_DUPLICATE_FIRST,
  SORT_MODE_COUNT
};

enum FilterFlags {
  FILTER_NONE = 0,
  FILTER_HIDE_HIDDEN = 1,
  FILTER_OPEN_ONLY = 2,
  FILTER_RISKY_ONLY = 4,
  FILTER_HIDE_LOW_SIGNAL = 8
};

// --- Risk reason IDs (explainability) ---
enum RiskReasonId {
  RISK_OPEN,
  RISK_WEP,
  RISK_WPA_LEGACY,
  RISK_WEAK_SIGNAL,
  RISK_VERY_WEAK_SIGNAL,
  RISK_SUSPICIOUS_SSID,
  RISK_CAPTIVE_PORTAL,
  RISK_DUPLICATE_SSID,
  RISK_HIDDEN,
  RISK_CONNECTIVITY_FAILED,
  RISK_CHANNEL_CONGESTED,
  RISK_EVIL_TWIN_SUSPECT,
  RISK_REASON_COUNT
};

// --- Risk label for display ---
enum RiskLabel { RISK_LOW = 0, RISK_MED = 1, RISK_HIGH = 2 };

// --- User-facing verdict (Simple Mode) ---
enum UserVerdict {
  VERDICT_SAFE,
  VERDICT_CAUTION,
  VERDICT_AVOID,
  VERDICT_LOGIN_REQUIRED,
  VERDICT_NO_INTERNET,
  VERDICT_TEST_NEEDED,
  VERDICT_COULD_NOT_TEST
};

// --- Evil twin / duplicate classification ---
enum DuplicateClass { DUP_NONE = 0, DUP_SAME_SSID = 1, DUP_SUSPICIOUS = 2, DUP_EVIL_TWIN_SUSPECT = 3 };

// --- Input events ---
enum InputEventType {
  EVT_NONE,
  EVT_TAP_B1,
  EVT_TAP_B2,
  EVT_LONG_B1,
  EVT_LONG_B2,
  EVT_CHORD
};

// --- UI view ---
enum UIView {
  VIEW_LIST,
  VIEW_DETAIL,
  VIEW_PORTAL_SUSPICIOUS,
  VIEW_QR,
  VIEW_ENV_SUMMARY,
  VIEW_SETTINGS,
  VIEW_HELP,
  VIEW_COUNT
};

// --- Network record (per-AP) ---
struct NetworkRecord {
  char     ssid[33];
  uint8_t  bssid[6];
  int8_t   rssi;
  AuthType auth;
  uint8_t  channel;
  bool     hidden;
  bool     duplicateSSID;
  uint8_t  riskScore;
  ConnectivityGrade grade;
  PortalResult     portalResult;
  uint16_t riskReasonsBitmask;  // bits for RiskReasonId
  bool     tested;
  bool     newThisScan;
  bool     goneNextScan;
  int8_t   rssiLastScan;        // for trend; -128 = N/A
  uint8_t  duplicateClass;      // DuplicateClass
  uint8_t  sameSSIDCount;       // how many APs share this SSID
  bool     sameSecurityAsGroup; // true if all with same SSID have same auth
  char     vendor[16];
  bool     possibleEvilTwin;
  int16_t  benchmarkAvgMs;      // -1 = not benchmarked
  int16_t  benchmarkJitterMs;   // -1 = not benchmarked
  char     portalUrl[96];       // redirect URL when captive portal detected
  char     portalDomain[48];    // extracted host for safety display
  bool     portalIsIP;          // host is numeric IP
  bool     portalIsHTTPS;       // protocol is https
  bool     portalIsLocalGateway; // 192.168/10/172.16-31
  bool     portalPathLooksLikePortal; // /login, /hotspot, etc.
  bool     portalBrandMismatch; // SSID vs domain mismatch
  bool     portalLongUrl;       // URL or query very long
  uint8_t  portalSafetyScore;   // 0-100
  PortalSafety portalSafety;   // SAFE / CAUTION / SUSPICIOUS
};

// --- Connectivity result (from test) ---
struct ConnectivityResult {
  ConnectivityGrade grade;
  PortalResult      portal;
  int32_t           associationMs;
  int32_t           dnsMs;
  int32_t           httpMs;
  bool              dnsOk;
  bool              httpOk;
  int               httpCode;
  bool              redirect;
  int16_t           benchmarkAvgMs;
  int16_t           benchmarkJitterMs;
  uint8_t           benchmarkPings;
  char              portalUrl[64];
};

// --- Scan record (one per scan) ---
struct ScanRecord {
  uint32_t       scanIndex;
  uint32_t       timestamp;
  uint16_t       networkCount;
  NetworkRecord  networks[MAX_NETWORKS];
  uint16_t       openCount;
  uint16_t       encryptedCount;
  uint16_t       hiddenCount;
  uint16_t       duplicateCount;
  uint8_t        bestChannel;
  uint8_t        worstChannel;
  uint8_t        congestionScore;
  int8_t         avgRssi;
  int8_t         minRssi;
  int8_t         maxRssi;
  uint16_t       portalDetectedCount;
  uint8_t        safestNetworkIndex;   // index in networks[]
  uint8_t        riskiestNetworkIndex;
};

// --- Session analytics (per boot) ---
struct SessionStats {
  uint32_t scansPerformed;
  uint8_t  mostCommonChannel;
  uint16_t avgNetworkCount;
  uint8_t  avgRisk;
  uint16_t portalsSeen;
  uint16_t openNetworksSeen;
  uint16_t duplicateSsidsSeen;
};

// --- Alert flags (bitmask) ---
enum AlertFlag {
  ALERT_OPEN = 1,
  ALERT_WEP = 2,
  ALERT_PORTAL = 4,
  ALERT_DUPLICATE_SSID = 8,
  ALERT_SUSPICIOUS_SSID = 16,
  ALERT_HIGH_CONGESTION = 32,
  ALERT_POSSIBLE_EVIL_TWIN = 64,
  ALERT_HIGH_RISK_OPEN = 128
};

// --- Input event ---
struct InputEvent {
  InputEventType type;
  uint16_t       durationMs;
};

// --- Settings (persisted) ---
struct SettingsRecord {
  uint8_t  historySize;
  bool     sessionOnly;
  bool     bssidInExport;
  uint32_t inactivityMs;
  uint8_t  brightness;
  SortMode sortMode;
  uint8_t  filterFlags;
  uint16_t connectTimeoutMs;
  uint16_t dnsTimeoutMs;
  uint16_t httpTimeoutMs;
  uint8_t  lowBatteryPct;
  bool     expertMode;
  bool     diagnosticSerial;
  bool     lowPowerMode;
  bool     alwaysMonitor;
  uint16_t monitorIntervalSec;
  bool     alertOnHighRisk;
  bool     exportSummaryOnly;
  bool     demoMode;
  uint16_t stabilityMonitorIntervalSec;
  uint8_t  lowBatteryDimPct;
  bool     lowBatterySkipTest;
  bool     lowBatteryReduceMonitor;
  bool     privacyMode;
  bool     highContrast;
};

#endif
