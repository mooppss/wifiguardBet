#include "WiFiScanner.h"
#include "OuiLookup.h"
#include <WiFi.h>
#include <Arduino.h>

WiFiScanner wifiScanner;

void WiFiScanner::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(200);
  hasResult_ = false;
  scanStartTime_ = 0;
  retryCount_ = 0;
}

AuthType WiFiScanner::mapAuth(int wifiAuth) {
  switch (wifiAuth) {
    case WIFI_AUTH_OPEN:           return AUTH_OPEN;
    case WIFI_AUTH_WEP:            return AUTH_WEP;
    case WIFI_AUTH_WPA_PSK:        return AUTH_WPA;
    case WIFI_AUTH_WPA2_PSK:       return AUTH_WPA2;
    case WIFI_AUTH_WPA_WPA2_PSK:   return AUTH_WPA_WPA2;
    case WIFI_AUTH_WPA2_ENTERPRISE: return AUTH_WPA2;
    case WIFI_AUTH_WPA3_PSK:       return AUTH_WPA3;
    case WIFI_AUTH_WPA2_WPA3_PSK:  return AUTH_WPA2_WPA3;
    default:                       return AUTH_UNKNOWN;
  }
}

void WiFiScanner::startScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(200);
  hasResult_ = false;
  scanStartTime_ = millis();
  WiFi.scanNetworks(true);
}

bool WiFiScanner::update() {
  if (hasResult_) return true;
  if (!scanStartTime_) return false;

  uint32_t elapsed = millis() - scanStartTime_;
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_RUNNING) {
    if (elapsed > SCAN_TIMEOUT_MS) {
      WiFi.scanDelete();
      n = 0;
    } else {
      return false;
    }
  }

  if (n == WIFI_SCAN_FAILED || (n < 0 && n != WIFI_SCAN_RUNNING)) {
    return false;
  }

  collectAndProcess();
  WiFi.scanDelete();
  scanStartTime_ = 0;
  hasResult_ = true;
  return true;
}

static NetworkRecord s_rawBuf[MAX_RAW_NETWORKS];

void WiFiScanner::collectAndProcess() {
  int n = WiFi.scanComplete();
  if (n <= 0) {
    result_.networkCount = 0;
    result_.timestamp = millis();
    result_.openCount = result_.encryptedCount = result_.hiddenCount = result_.duplicateCount = 0;
    return;
  }

  NetworkRecord* raw = s_rawBuf;
  uint16_t count = 0;
  for (int i = 0; i < n && count < MAX_RAW_NETWORKS; i++) {
    NetworkRecord& r = raw[count];
    memset(&r, 0, sizeof(r));
    strncpy(r.ssid, WiFi.SSID(i).c_str(), 32);
    r.ssid[32] = '\0';
    memcpy(r.bssid, WiFi.BSSID(i), 6);
    r.rssi = WiFi.RSSI(i);
    r.auth = mapAuth(WiFi.encryptionType(i));
    r.channel = WiFi.channel(i);
    if (r.channel == 0) r.channel = 255;
    r.hidden = (r.ssid[0] == '\0');
    r.grade = (r.auth != AUTH_OPEN && r.auth != AUTH_UNKNOWN) ? GRADE_PROTECTED : GRADE_UNTESTED;
    r.portalResult = (r.auth != AUTH_OPEN && r.auth != AUTH_UNKNOWN) ? PORTAL_CREDENTIAL_REQUIRED : PORTAL_UNKNOWN;
    r.rssiLastScan = -128;
    r.sameSSIDCount = 1;
    r.sameSecurityAsGroup = true;
    r.benchmarkAvgMs = -1;
    r.benchmarkJitterMs = -1;
    count++;
  }

  dedupeByBssid(raw, count);
  markDuplicateSsids(raw, count);
  computeDuplicateIntelligence(raw, count);
  limitAndSort(raw, count);

  result_.networkCount = count;
  result_.timestamp = millis();
  result_.openCount = result_.encryptedCount = result_.hiddenCount = result_.duplicateCount = 0;
  result_.minRssi = 0;
  result_.maxRssi = -128;
  int32_t sumRssi = 0;
  for (uint16_t i = 0; i < count; i++) {
    result_.networks[i] = raw[i];
    ouiLookup(result_.networks[i]);
    if (raw[i].auth == AUTH_OPEN) result_.openCount++;
    else result_.encryptedCount++;
    if (raw[i].hidden) result_.hiddenCount++;
    if (raw[i].duplicateSSID) result_.duplicateCount++;
    if (raw[i].rssi > result_.maxRssi) result_.maxRssi = raw[i].rssi;
    if (raw[i].rssi < result_.minRssi || result_.minRssi == 0) result_.minRssi = raw[i].rssi;
    sumRssi += raw[i].rssi;
  }
  result_.avgRssi = (count > 0) ? (int8_t)(sumRssi / count) : 0;
  result_.bestChannel = result_.worstChannel = 0;
  result_.congestionScore = 0;
}

void WiFiScanner::markNewNetworks(ScanRecord& scan, const ScanRecord* previous) {
#if FEATURE_NEW_GONE
  if (!previous || previous->networkCount == 0) return;
  for (uint16_t i = 0; i < scan.networkCount; i++) {
    bool found = false;
    for (uint16_t j = 0; j < previous->networkCount; j++) {
      if (memcmp(scan.networks[i].bssid, previous->networks[j].bssid, 6) == 0) {
        found = true;
        break;
      }
    }
    if (!found) scan.networks[i].newThisScan = true;
  }
#else
  (void)scan;
  (void)previous;
#endif
}

void WiFiScanner::fillRssiFromPrevious(ScanRecord& scan, const ScanRecord* previous) {
#if FEATURE_NEW_GONE
  if (!previous || previous->networkCount == 0) return;
  for (uint16_t i = 0; i < scan.networkCount; i++) {
    for (uint16_t j = 0; j < previous->networkCount; j++) {
      if (memcmp(scan.networks[i].bssid, previous->networks[j].bssid, 6) == 0) {
        scan.networks[i].rssiLastScan = previous->networks[j].rssi;
        break;
      }
    }
  }
#else
  (void)scan;
  (void)previous;
#endif
}

void WiFiScanner::dedupeByBssid(NetworkRecord* list, uint16_t& n) {
  for (uint16_t i = 0; i < n; i++) {
    for (uint16_t j = i + 1; j < n; j++) {
      if (memcmp(list[i].bssid, list[j].bssid, 6) == 0) {
        if (list[j].rssi > list[i].rssi) list[i] = list[j];
        for (uint16_t k = j; k < n - 1; k++) list[k] = list[k + 1];
        n--;
        j--;
      }
    }
  }
}

void WiFiScanner::markDuplicateSsids(NetworkRecord* list, uint16_t n) {
  for (uint16_t i = 0; i < n; i++) {
    for (uint16_t j = 0; j < n; j++) {
      if (i != j && strcmp(list[i].ssid, list[j].ssid) == 0) {
        list[i].duplicateSSID = true;
        list[j].duplicateSSID = true;
        break;
      }
    }
  }
}

void WiFiScanner::computeDuplicateIntelligence(NetworkRecord* list, uint16_t n) {
  for (uint16_t i = 0; i < n; i++) {
    uint8_t sameCount = 0;
    bool sameAuth = true;
    AuthType firstAuth = list[i].auth;
    bool hasOpen = false, hasProtected = false;
    for (uint16_t j = 0; j < n; j++) {
      if (strcmp(list[i].ssid, list[j].ssid) != 0) continue;
      sameCount++;
      if (list[j].auth != firstAuth) sameAuth = false;
      if (list[j].auth == AUTH_OPEN) hasOpen = true;
      else hasProtected = true;
    }
    list[i].sameSSIDCount = sameCount;
    list[i].sameSecurityAsGroup = sameAuth;
    if (sameCount <= 1) {
      list[i].duplicateClass = DUP_NONE;
      list[i].possibleEvilTwin = false;
    } else {
      list[i].duplicateClass = DUP_SAME_SSID;
      if (!sameAuth) list[i].duplicateClass = DUP_SUSPICIOUS;
      if (hasOpen && hasProtected) list[i].possibleEvilTwin = true;
      else if (!sameAuth && sameCount >= 2) list[i].possibleEvilTwin = true;
      if (list[i].possibleEvilTwin) list[i].duplicateClass = DUP_EVIL_TWIN_SUSPECT;
    }
  }
}

void WiFiScanner::limitAndSort(NetworkRecord* list, uint16_t& n) {
  // Sort by RSSI descending (strongest first)
  for (uint16_t i = 0; i < n; i++) {
    for (uint16_t j = i + 1; j < n; j++) {
      if (list[j].rssi > list[i].rssi) {
        NetworkRecord t = list[i];
        list[i] = list[j];
        list[j] = t;
      }
    }
  }
  if (n > MAX_NETWORKS) n = MAX_NETWORKS;
}

void WiFiScanner::getResult(ScanRecord& out) const {
  out = result_;
}
