#include "SessionStats.h"
#include <string.h>

static SessionStats s;
static uint32_t scanCountAccum = 0;
static uint32_t channelSum = 0;
static uint32_t riskSum = 0;

void sessionStatsInit() {
  memset(&s, 0, sizeof(s));
  scanCountAccum = 0;
  channelSum = 0;
  riskSum = 0;
}

void sessionStatsOnScan(const ScanRecord& scan) {
  s.scansPerformed++;
  s.openNetworksSeen += scan.openCount;
  s.duplicateSsidsSeen += scan.duplicateCount;
  s.portalsSeen += scan.portalDetectedCount;
  if (scan.networkCount > 0) {
    scanCountAccum += scan.networkCount;
    for (uint16_t i = 0; i < scan.networkCount; i++)
      riskSum += scan.networks[i].riskScore;
    s.avgRisk = (uint8_t)(riskSum / (scanCountAccum > 0 ? scanCountAccum : 1));
  }
  s.avgNetworkCount = (uint16_t)(scanCountAccum / (s.scansPerformed > 0 ? s.scansPerformed : 1));
  uint8_t ch = scan.worstChannel;
  if (ch >= 1 && ch <= 14) channelSum += ch;
  s.mostCommonChannel = (s.scansPerformed > 0 && channelSum > 0) ? (uint8_t)(channelSum / s.scansPerformed) : 0;
}

void sessionStatsGet(SessionStats& out) {
  out = s;
}
