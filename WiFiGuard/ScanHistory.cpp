#include "ScanHistory.h"
#include "Settings.h"
#include <Preferences.h>
#include <string.h>

#define NVS_NAMESPACE_SCAN "wguard"
#define NVS_SCAN_CHUNK    496

static Preferences nvsScan;

ScanHistory scanHistory;

void ScanHistory::begin() {
  head_ = 0;
  count_ = 0;
  hasCurrent_ = false;
  memset(&currentScan_, 0, sizeof(currentScan_));
  if (settings.get().sessionOnly) {
    clear();
    return;
  }
  loadFromNvs();
}

void ScanHistory::loadFromNvs() {
  if (!nvsScan.begin(NVS_NAMESPACE_SCAN, true)) return;
  uint8_t n = nvsScan.getUChar("cnt", 0);
  if (n == 0 || n > HISTORY_NVS_MAX) { nvsScan.end(); return; }
  size_t numChunks = (sizeof(ScanRecord) + NVS_SCAN_CHUNK - 1) / NVS_SCAN_CHUNK;
  for (uint8_t i = 0; i < n; i++) {
    for (size_t c = 0; c < numChunks; c++) {
      char key[8];
      snprintf(key, sizeof(key), "s%u_%u", (unsigned)i, (unsigned)c);
      size_t len = nvsScan.getBytes(key, (char*)&slots_[i] + c * NVS_SCAN_CHUNK, NVS_SCAN_CHUNK);
      if (len == 0) break;
    }
  }
  count_ = n;
  head_ = count_ > 0 ? count_ - 1 : 0;
  nvsScan.end();
}

void ScanHistory::persist() {
  if (count_ == 0) return;
  uint8_t n = count_ <= HISTORY_NVS_MAX ? count_ : (uint8_t)HISTORY_NVS_MAX;
  uint8_t start = count_ - n;
  size_t numChunks = (sizeof(ScanRecord) + NVS_SCAN_CHUNK - 1) / NVS_SCAN_CHUNK;
  if (!nvsScan.begin(NVS_NAMESPACE_SCAN, false)) return;
  nvsScan.putUChar("cnt", n);
  for (uint8_t i = 0; i < n; i++) {
    const ScanRecord* sr = &slots_[start + i];
    for (size_t c = 0; c < numChunks; c++) {
      char key[8];
      snprintf(key, sizeof(key), "s%u_%u", (unsigned)i, (unsigned)c);
      size_t off = c * NVS_SCAN_CHUNK;
      size_t len = (off + NVS_SCAN_CHUNK <= sizeof(ScanRecord)) ? NVS_SCAN_CHUNK : (sizeof(ScanRecord) - off);
      nvsScan.putBytes(key, (const char*)sr + off, len);
    }
  }
  nvsScan.end();
}

void ScanHistory::setCurrentScan(const ScanRecord& scan) {
  currentScan_ = scan;
  hasCurrent_ = true;
}

void ScanHistory::push(const ScanRecord& scan) {
  if (count_ < HISTORY_SIZE) {
    slots_[count_] = scan;
    count_++;
  } else {
    for (uint8_t i = 0; i < count_ - 1; i++)
      slots_[i] = slots_[i + 1];
    slots_[count_ - 1] = scan;
  }
  head_ = count_ - 1;
  if (!settings.get().sessionOnly)
    persist();
}

bool ScanHistory::getLatest(ScanRecord& out) const {
  if (hasCurrent_) {
    out = currentScan_;
    return true;
  }
  if (count_ == 0) return false;
  out = slots_[count_ - 1];
  return true;
}

bool ScanHistory::getByIndex(int index, ScanRecord& out) const {
  if (count_ == 0) return false;
  if (index < 0) index = 0;
  if (index >= (int)count_) index = count_ - 1;
  int i = count_ - 1 - index;
  out = slots_[i];
  return true;
}

void ScanHistory::clear() {
  count_ = 0;
  head_ = 0;
  hasCurrent_ = false;
}

ScanRecord* ScanHistory::getCurrentScanForUpdate() {
  if (hasCurrent_) return &currentScan_;
  if (count_ > 0) return &slots_[count_ - 1];
  return nullptr;
}

void ScanHistory::updateNetworkConnectivity(uint16_t networkIndex, ConnectivityGrade g, PortalResult p) {
  ScanRecord* r = getCurrentScanForUpdate();
  if (r && networkIndex < r->networkCount) {
    r->networks[networkIndex].grade = g;
    r->networks[networkIndex].portalResult = p;
    r->networks[networkIndex].tested = true;
  }
}

void ScanHistory::updateNetworkBenchmark(uint16_t networkIndex, int16_t avgMs, int16_t jitterMs) {
  ScanRecord* r = getCurrentScanForUpdate();
  if (r && networkIndex < r->networkCount) {
    r->networks[networkIndex].benchmarkAvgMs = avgMs;
    r->networks[networkIndex].benchmarkJitterMs = jitterMs;
  }
}

void ScanHistory::markGoneInPreviousScan(const ScanRecord& newScan) {
  ScanRecord* prev = getCurrentScanForUpdate();
  if (!prev || prev->networkCount == 0) return;
  for (uint16_t i = 0; i < prev->networkCount; i++) {
    bool found = false;
    for (uint16_t j = 0; j < newScan.networkCount; j++) {
      if (memcmp(prev->networks[i].bssid, newScan.networks[j].bssid, 6) == 0) {
        found = true;
        break;
      }
    }
    if (!found) prev->networks[i].goneNextScan = true;
  }
}
