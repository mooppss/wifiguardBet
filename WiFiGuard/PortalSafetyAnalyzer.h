#ifndef PORTAL_SAFETY_ANALYZER_H
#define PORTAL_SAFETY_ANALYZER_H

#include "Config.h"
#include "Types.h"

// Captive portal safety analysis: parse redirect URL, score heuristics, classify SAFE/CAUTION/SUSPICIOUS.
// Safety: WiFiGuard only analyzes and informs. It never auto-submits portal forms, stores credentials,
// bypasses captive portals, auto-accepts terms, or evades access controls. All "open in browser" or
// "show QR" actions are user-initiated.
class PortalSafetyAnalyzer {
public:
  // Run full analysis and write results into n (portalUrl, portalDomain, booleans, portalSafetyScore, portalSafety).
  void evaluatePortalSafety(const char* portalUrl, const char* ssid, NetworkRecord* n);

private:
  void parsePortalUrl(const char* url, NetworkRecord* n);
  void extractDomain(const char* url, char* domainOut, int maxLen);
  bool detectBrandMismatch(const char* ssid, const char* domain);
  static bool isLocalGatewayIP(const char* host);
  static bool isIPv4(const char* host);
  static bool pathLooksLikePortal(const char* path);
};

extern PortalSafetyAnalyzer portalSafetyAnalyzer;

#endif
