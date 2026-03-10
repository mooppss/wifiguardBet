#include "PortalSafetyAnalyzer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

PortalSafetyAnalyzer portalSafetyAnalyzer;

// Copy at most maxLen-1 chars, null-terminate
static void copyStr(char* dest, const char* src, int maxLen) {
  if (maxLen <= 0) return;
  int i = 0;
  while (src[i] && i < maxLen - 1) {
    dest[i] = src[i];
    i++;
  }
  dest[i] = '\0';
}

// Case-insensitive compare of two strings, true if a equals b
static bool strEqualCI(const char* a, const char* b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    a++;
    b++;
  }
  return *a == *b;
}

// True if path starts with /segment (case-insensitive); path must start with /
static bool pathStartsWith(const char* path, const char* segment) {
  if (!path || path[0] != '/') return false;
  path++;
  while (*path && *segment) {
    if (tolower((unsigned char)*path) != tolower((unsigned char)*segment)) return false;
    path++;
    segment++;
  }
  if (*segment) return false;
  return *path == '\0' || *path == '/' || *path == '?';
}

bool PortalSafetyAnalyzer::pathLooksLikePortal(const char* path) {
  if (!path) return false;
  const char* patterns[] = {
    "/login", "/hotspot", "/captive", "/wifi", "/generate_204", "/redirect", "/index.html"
  };
  for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
    if (pathStartsWith(path, patterns[i] + 1)) return true;  // +1 to skip leading /
  }
  return false;
}

bool PortalSafetyAnalyzer::isIPv4(const char* host) {
  if (!host || !*host) return false;
  int dots = 0;
  while (*host) {
    if (*host == '.') dots++;
    else if (!isdigit((unsigned char)*host)) return false;
    host++;
  }
  return dots == 3;
}

// Parse 192.168.x.x, 10.x.x.x, 172.16.x.x - 172.31.x.x
bool PortalSafetyAnalyzer::isLocalGatewayIP(const char* host) {
  if (!host || !*host) return false;
  unsigned int a = 0, b = 0, c = 0, d = 0;
  if (sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
  if (a == 192 && b == 168) return true;
  if (a == 10) return true;
  if (a == 172 && b >= 16 && b <= 31) return true;
  return false;
}

void PortalSafetyAnalyzer::extractDomain(const char* url, char* domainOut, int maxLen) {
  if (!url || !domainOut || maxLen <= 0) { if (domainOut && maxLen > 0) domainOut[0] = '\0'; return; }
  domainOut[0] = '\0';
  // Skip protocol
  if (strncmp(url, "https://", 8) == 0) url += 8;
  else if (strncmp(url, "http://", 7) == 0) url += 7;
  const char* hostStart = url;
  const char* hostEnd = strchr(url, '/');
  if (!hostEnd) hostEnd = url + strlen(url);
  int len = (int)(hostEnd - hostStart);
  if (len <= 0 || len >= maxLen) return;
  memcpy(domainOut, hostStart, len);
  domainOut[len] = '\0';
}

void PortalSafetyAnalyzer::parsePortalUrl(const char* url, NetworkRecord* n) {
  if (!n) return;
  n->portalDomain[0] = '\0';
  n->portalIsIP = false;
  n->portalIsHTTPS = false;
  n->portalIsLocalGateway = false;
  n->portalPathLooksLikePortal = false;
  n->portalBrandMismatch = false;
  n->portalLongUrl = false;
  if (!url || !url[0]) return;

  int urlLen = (int)strlen(url);
  if (urlLen >= 96) urlLen = 95;
  memcpy(n->portalUrl, url, urlLen);
  n->portalUrl[urlLen] = '\0';

  // Long URL / long query
  const char* q = strchr(url, '?');
  if (urlLen > 80) n->portalLongUrl = true;
  else if (q && (int)(strlen(q) - 1) > 40) n->portalLongUrl = true;

  bool hasProtocol = (strncmp(url, "https://", 8) == 0);
  if (hasProtocol) n->portalIsHTTPS = true;
  else if (strncmp(url, "http://", 7) == 0) n->portalIsHTTPS = false;

  const char* hostStart = url;
  if (strncmp(url, "https://", 8) == 0) hostStart = url + 8;
  else if (strncmp(url, "http://", 7) == 0) hostStart = url + 7;
  const char* pathStart = strchr(hostStart, '/');
  const char* hostEnd = pathStart ? pathStart : hostStart + strlen(hostStart);
  int hostLen = (int)(hostEnd - hostStart);
  if (hostLen >= 48) hostLen = 47;
  memcpy(n->portalDomain, hostStart, hostLen);
  n->portalDomain[hostLen] = '\0';

  n->portalIsIP = isIPv4(n->portalDomain);
  if (n->portalIsIP)
    n->portalIsLocalGateway = isLocalGatewayIP(n->portalDomain);

  if (pathStart && pathStart[0] == '/')
    n->portalPathLooksLikePortal = pathLooksLikePortal(pathStart);
}

bool PortalSafetyAnalyzer::detectBrandMismatch(const char* ssid, const char* domain) {
  if (!ssid || !ssid[0] || !domain || !domain[0]) return false;
  char domainRoot[32];
  const char* lastDot = strrchr(domain, '.');
  if (lastDot && lastDot != domain) {
    int len = (int)(lastDot - domain);
    if (len >= 32) len = 31;
    memcpy(domainRoot, domain, len);
    domainRoot[len] = '\0';
  } else {
    copyStr(domainRoot, domain, sizeof(domainRoot));
  }
  // Extract first "word" from SSID (split on _, -, space)
  char ssidLower[33];
  for (int i = 0; ssid[i] && i < 32; i++)
    ssidLower[i] = (char)tolower((unsigned char)ssid[i]);
  ssidLower[32] = '\0';
  char domainLower[32];
  for (int i = 0; domainRoot[i] && i < 31; i++)
    domainLower[i] = (char)tolower((unsigned char)domainRoot[i]);
  domainLower[31] = '\0';

  // Check if any segment of SSID appears in domain root or vice versa
  const char* p = ssidLower;
  while (*p) {
    char word[24];
    int w = 0;
    while (*p && *p != '_' && *p != '-' && *p != ' ' && w < 23) word[w++] = *p++;
    word[w] = '\0';
    while (*p == '_' || *p == '-' || *p == ' ') p++;
    if (w >= 2 && strstr(domainLower, word)) return false;  // match found
  }
  if (strlen(domainLower) >= 2 && strstr(ssidLower, domainLower)) return false;  // domain root in SSID
  return true;  // no meaningful overlap
}

void PortalSafetyAnalyzer::evaluatePortalSafety(const char* portalUrl, const char* ssid, NetworkRecord* n) {
  if (!n) return;
  parsePortalUrl(portalUrl, n);
  if (!n->portalUrl[0]) return;

  n->portalBrandMismatch = detectBrandMismatch(ssid, n->portalDomain);

  int score = 50;

  // SAFE signals
  if (!n->portalIsIP) score += 10;
  if (n->portalIsHTTPS) score += 10;
  if (n->portalIsLocalGateway) score += 15;
  if (n->portalPathLooksLikePortal) score += 10;
  if (!n->portalBrandMismatch && n->portalDomain[0]) score += 10;

  // RISK signals
  if (n->portalIsIP && !n->portalIsLocalGateway) score -= 25;
  if (n->portalBrandMismatch) score -= 20;
  if (n->portalLongUrl) score -= 10;
  if (!n->portalIsHTTPS) score -= 5;
  if (!n->portalIsLocalGateway && n->portalDomain[0]) score -= 10;  // external domain

  if (score < 0) score = 0;
  if (score > 100) score = 100;
  n->portalSafetyScore = (uint8_t)score;

  if (score >= 70) n->portalSafety = PORTAL_SAFE;
  else if (score >= 40) n->portalSafety = PORTAL_CAUTION;
  else n->portalSafety = PORTAL_SUSPICIOUS;
}
