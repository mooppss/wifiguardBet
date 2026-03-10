#ifndef ENVIRONMENTANALYSIS_H
#define ENVIRONMENTANALYSIS_H

#include "Config.h"
#include "Types.h"

#define WIFI_CHANNELS 14
#define ENV_SUMMARY_LEN 80

#define ENV_CHANNEL_RANK_LEN 16
struct EnvStats {
  uint16_t totalNetworks;
  uint16_t openCount;
  uint16_t encryptedCount;
  uint16_t hiddenCount;
  uint16_t duplicateSsids;
  uint16_t portalDetectedCount;
  uint8_t  channelCounts[WIFI_CHANNELS + 1];  // 1-14, 0 unused
  uint8_t  congestionPerChannel[WIFI_CHANNELS + 1];
  uint8_t  channelRanking[ENV_CHANNEL_RANK_LEN];  // 1..14, best first
  uint8_t  bestChannel;
  uint8_t  worstChannel;
  uint8_t  overallCongestionScore;
  uint8_t  safestNetworkIndex;
  uint8_t  riskiestNetworkIndex;
  char     summary[ENV_SUMMARY_LEN];
  char     bestChannelSuggestion[24];
  char     worstChannelWarning[24];
};

class EnvironmentAnalysis {
public:
  void compute(const ScanRecord& scan, EnvStats& out);
};

extern EnvironmentAnalysis environmentAnalysis;

#endif
