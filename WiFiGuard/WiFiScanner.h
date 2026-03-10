#ifndef WIFISCANNER_H
#define WIFISCANNER_H

#include "Config.h"
#include "Types.h"

// WiFi scan pipeline: async scan, collect, dedupe, duplicate-SSID, limit, output ScanRecord.
class WiFiScanner {
public:
  void begin();
  void startScan();  // non-blocking; sets phase to 1
  bool update();     // call every loop; returns true when scan done (phase 2)
  void getResult(ScanRecord& out) const;
  bool hasResult() const { return hasResult_; }
  uint32_t getScanStartTime() const { return scanStartTime_; }
  void markNewNetworks(ScanRecord& scan, const ScanRecord* previous);
  void fillRssiFromPrevious(ScanRecord& scan, const ScanRecord* previous);
  void computeDuplicateIntelligence(NetworkRecord* list, uint16_t n);

private:
  void collectAndProcess();
  AuthType mapAuth(int wifiAuth);
  void dedupeByBssid(NetworkRecord* list, uint16_t& n);
  void markDuplicateSsids(NetworkRecord* list, uint16_t n);
  void limitAndSort(NetworkRecord* list, uint16_t& n);

  ScanRecord result_;
  bool       hasResult_;
  uint32_t   scanStartTime_;
  uint8_t    retryCount_;
};

extern WiFiScanner wifiScanner;

#endif
