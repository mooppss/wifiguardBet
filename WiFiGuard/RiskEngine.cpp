#include "RiskEngine.h"
#include <string.h>

RiskEngine riskEngine;

static const char* const reasonStrings[] = {
  "Open",
  "WEP",
  "WPA legacy",
  "Weak signal",
  "Very weak signal",
  "Suspicious SSID",
  "Captive portal",
  "Duplicate SSID",
  "Hidden",
  "Connectivity failed",
  "Channel congested",
  "Evil twin?"
};

// Suspicious SSID keywords (lowercase)
static const char* suspiciousKeywords[] = {
  "free", "guest", "airport", "starbucks", "hotel", "public", "wifi", "open"
};
static const uint8_t numSuspicious = 8;

static bool hasSuspiciousKeyword(const char* ssid) {
  char lower[33];
  size_t len = strlen(ssid);
  if (len >= 33) len = 32;
  for (size_t i = 0; i < len; i++) {
    char c = ssid[i];
    if (c >= 'A' && c <= 'Z') c += 32;
    lower[i] = c;
  }
  lower[len] = '\0';
  for (uint8_t k = 0; k < numSuspicious; k++) {
    if (strstr(lower, suspiciousKeywords[k]) != nullptr)
      return true;
  }
  return false;
}

const char* RiskEngine::getReasonString(RiskReasonId id) const {
  if (id >= RISK_REASON_COUNT) return "";
  return reasonStrings[id];
}

RiskLabel RiskEngine::getRiskLabel(uint8_t score) const {
  if (score >= 65) return RISK_HIGH;
  if (score >= 35) return RISK_MED;
  return RISK_LOW;
}

void RiskEngine::computeOne(NetworkRecord& net, const uint8_t* channelCongestion) {
  uint16_t bitmask = 0;
  int score = 0;

  switch (net.auth) {
    case AUTH_OPEN:   score += 25; bitmask |= (1 << RISK_OPEN); break;
    case AUTH_WEP:    score += 30; bitmask |= (1 << RISK_WEP); break;
    case AUTH_WPA:
    case AUTH_WPA_WPA2: score += 15; bitmask |= (1 << RISK_WPA_LEGACY); break;
    default: break;
  }

  if (net.rssi <= -90) {
    score += 15;
    bitmask |= (1 << RISK_VERY_WEAK_SIGNAL);
  } else if (net.rssi <= -80) {
    score += 10;
    bitmask |= (1 << RISK_WEAK_SIGNAL);
  }

  if (net.ssid[0] && hasSuspiciousKeyword(net.ssid)) {
    score += 15;
    bitmask |= (1 << RISK_SUSPICIOUS_SSID);
  }

  if (net.portalResult == PORTAL_REDIRECT_LOGIN || net.portalResult == PORTAL_INTERCEPT) {
    score += 15;
    bitmask |= (1 << RISK_CAPTIVE_PORTAL);
  }

  if (net.grade == GRADE_FAILED || net.grade == GRADE_OFFLINE) {
    score += 5;
    bitmask |= (1 << RISK_CONNECTIVITY_FAILED);
  }

  if (net.duplicateSSID) {
    score += 20;
    bitmask |= (1 << RISK_DUPLICATE_SSID);
  }
  if (net.possibleEvilTwin) {
    score += 25;
    bitmask |= (1 << RISK_EVIL_TWIN_SUSPECT);
  }

  if (net.hidden) {
    score += 5;
    bitmask |= (1 << RISK_HIDDEN);
  }

  if (channelCongestion && net.channel <= 14) {
    uint8_t c = channelCongestion[net.channel];
    if (c >= 5) {
      score += 10;
      bitmask |= (1 << RISK_CHANNEL_CONGESTED);
    }
  }

  if (score > RISK_SCORE_MAX) score = RISK_SCORE_MAX;
  net.riskScore = (uint8_t)score;
  net.riskReasonsBitmask = bitmask;
}

void RiskEngine::compute(ScanRecord& scan, const uint8_t* channelCongestion) {
  for (uint16_t i = 0; i < scan.networkCount; i++) {
    computeOne(scan.networks[i], channelCongestion);
  }
}
