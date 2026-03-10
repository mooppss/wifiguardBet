#ifndef RISKENGINE_H
#define RISKENGINE_H

#include "Config.h"
#include "Types.h"

// Computes risk score 0-100 and risk reason bitmask for each network.
// Uses env data (e.g. per-channel congestion) when available.
class RiskEngine {
public:
  void compute(ScanRecord& scan, const uint8_t* channelCongestion = nullptr);
  void computeOne(NetworkRecord& net, const uint8_t* channelCongestion = nullptr);
  const char* getReasonString(RiskReasonId id) const;
  RiskLabel getRiskLabel(uint8_t score) const;
};

extern RiskEngine riskEngine;

#endif
