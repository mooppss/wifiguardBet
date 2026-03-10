#ifndef SCANHISTORY_H
#define SCANHISTORY_H

#include "Config.h"
#include "Types.h"

class ScanHistory {
public:
  void begin();
  void push(const ScanRecord& scan);
  bool getLatest(ScanRecord& out) const;
  bool getByIndex(int index, ScanRecord& out) const;  // 0 = latest
  uint16_t count() const { return count_; }
  void clear();
  void setCurrentScan(const ScanRecord& scan);
  const ScanRecord* getCurrentScan() const { return hasCurrent_ ? &currentScan_ : (count_ > 0 ? &slots_[count_ - 1] : nullptr); }
  const ScanRecord* getLatestPtr() const { return count_ > 0 ? &slots_[count_ - 1] : nullptr; }
  ScanRecord* getCurrentScanForUpdate();
  void markCurrentStored() { hasCurrent_ = false; }
  void updateNetworkConnectivity(uint16_t networkIndex, ConnectivityGrade g, PortalResult p);
  void updateNetworkBenchmark(uint16_t networkIndex, int16_t avgMs, int16_t jitterMs);
  void markGoneInPreviousScan(const ScanRecord& newScan);

private:
  void persist();
  void loadFromNvs();

  ScanRecord slots_[HISTORY_SIZE];
  ScanRecord currentScan_;
  bool       hasCurrent_;
  uint8_t   head_;
  uint8_t   count_;
};

extern ScanHistory scanHistory;

#endif
