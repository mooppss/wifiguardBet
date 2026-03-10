#include "EnvironmentAnalysis.h"
#include <string.h>
#include <stdio.h>

EnvironmentAnalysis environmentAnalysis;

void EnvironmentAnalysis::compute(const ScanRecord& scan, EnvStats& out) {
  memset(&out, 0, sizeof(out));
  out.totalNetworks = scan.networkCount;
  out.openCount = scan.openCount;
  out.encryptedCount = scan.encryptedCount;
  out.hiddenCount = scan.hiddenCount;
  out.duplicateSsids = scan.duplicateCount;

  for (int i = 0; i <= WIFI_CHANNELS; i++) {
    out.channelCounts[i] = 0;
    out.congestionPerChannel[i] = 0;
  }

  for (uint16_t i = 0; i < scan.networkCount; i++) {
    uint8_t ch = scan.networks[i].channel;
    if (ch >= 1 && ch <= WIFI_CHANNELS) {
      out.channelCounts[ch]++;
      out.congestionPerChannel[ch] += (scan.networks[i].rssi > -70 ? 2 : 1);
    }
  }

  out.portalDetectedCount = 0;
  for (uint16_t i = 0; i < scan.networkCount; i++) {
    if (scan.networks[i].grade == GRADE_PORTAL ||
        scan.networks[i].portalResult == PORTAL_REDIRECT_LOGIN ||
        scan.networks[i].portalResult == PORTAL_INTERCEPT)
      out.portalDetectedCount++;
  }

  out.safestNetworkIndex = 0;
  out.riskiestNetworkIndex = 0;
  uint8_t lowRisk = 101;
  uint8_t highRisk = 0;
  for (uint16_t i = 0; i < scan.networkCount; i++) {
    if (scan.networks[i].rssi >= -85 && scan.networks[i].riskScore < lowRisk) {
      lowRisk = scan.networks[i].riskScore;
      out.safestNetworkIndex = (uint8_t)i;
    }
    if (scan.networks[i].riskScore > highRisk) {
      highRisk = scan.networks[i].riskScore;
      out.riskiestNetworkIndex = (uint8_t)i;
    }
  }

  uint8_t minC = 255, maxC = 0;
  for (int ch = 1; ch <= WIFI_CHANNELS; ch++) {
    if (out.channelCounts[ch] > maxC) {
      maxC = out.channelCounts[ch];
      out.worstChannel = (uint8_t)ch;
    }
    if (out.channelCounts[ch] < minC && out.channelCounts[ch] > 0) {
      minC = out.channelCounts[ch];
      out.bestChannel = (uint8_t)ch;
    }
  }
  if (minC == 255) out.bestChannel = 1;
  if (maxC == 0) out.worstChannel = 6;

  for (int i = 0; i < ENV_CHANNEL_RANK_LEN; i++) out.channelRanking[i] = 0;
  for (int r = 0; r < WIFI_CHANNELS && r < ENV_CHANNEL_RANK_LEN; r++) {
    uint8_t bestCh = 0;
    uint8_t bestVal = 255;
    for (int ch = 1; ch <= WIFI_CHANNELS; ch++) {
      bool used = false;
      for (int j = 0; j < r; j++) if (out.channelRanking[j] == ch) { used = true; break; }
      if (!used && out.channelCounts[ch] < bestVal) {
        bestVal = out.channelCounts[ch];
        bestCh = (uint8_t)ch;
      }
    }
    out.channelRanking[r] = bestCh ? bestCh : (r + 1);
  }

  out.overallCongestionScore = 0;
  for (int ch = 1; ch <= WIFI_CHANNELS; ch++) {
    if (out.congestionPerChannel[ch] > out.overallCongestionScore)
      out.overallCongestionScore = out.congestionPerChannel[ch];
  }
  if (out.overallCongestionScore > 100) out.overallCongestionScore = 100;

  snprintf(out.bestChannelSuggestion, sizeof(out.bestChannelSuggestion) - 1,
    "Use ch%u (least busy)", (unsigned)out.bestChannel);
  out.bestChannelSuggestion[sizeof(out.bestChannelSuggestion) - 1] = '\0';
  snprintf(out.worstChannelWarning, sizeof(out.worstChannelWarning) - 1,
    "Avoid ch%u (busiest)", (unsigned)out.worstChannel);
  out.worstChannelWarning[sizeof(out.worstChannelWarning) - 1] = '\0';

  snprintf(out.summary, ENV_SUMMARY_LEN - 1,
    "%u open, %u enc, %u hid, %u dup, %u portal; ch%u busy",
    (unsigned)out.openCount, (unsigned)out.encryptedCount,
    (unsigned)out.hiddenCount, (unsigned)out.duplicateSsids,
    (unsigned)out.portalDetectedCount, (unsigned)out.worstChannel);
  out.summary[ENV_SUMMARY_LEN - 1] = '\0';
}
