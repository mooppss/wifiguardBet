#include "Export.h"
#include "ScanHistory.h"
#include "Settings.h"
#include "EnvironmentAnalysis.h"
#include <Arduino.h>
#include <stdio.h>

Export exportModule;

void Export::begin() {}

static void printBssid(const uint8_t* bssid, bool mask) {
  if (mask) {
    Serial.print("xx:xx:xx:xx:xx:xx");
  } else {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    Serial.print(buf);
  }
}

void Export::exportCurrentScanCSV(bool maskBssid) {
  bool mask = maskBssid || !settings.get().bssidInExport;
  const ScanRecord* scan = scanHistory.getCurrentScan();
  if (!scan) return;
  Serial.println("scanIndex,networkIndex,SSID,BSSID,RSSI,auth,channel,risk,grade,portal,hidden,duplicate");
  for (uint16_t i = 0; i < scan->networkCount; i++) {
    const NetworkRecord& n = scan->networks[i];
    Serial.print(scan->scanIndex); Serial.print(',');
    Serial.print(i); Serial.print(',');
    Serial.print("\""); Serial.print(n.ssid); Serial.print("\",");
    printBssid(n.bssid, mask); Serial.print(',');
    Serial.print(n.rssi); Serial.print(',');
    Serial.print((int)n.auth); Serial.print(',');
    Serial.print(n.channel); Serial.print(',');
    Serial.print(n.riskScore); Serial.print(',');
    Serial.print((int)n.grade); Serial.print(',');
    Serial.print((int)n.portalResult); Serial.print(',');
    Serial.print(n.hidden ? 1 : 0); Serial.print(',');
    Serial.println(n.duplicateSSID ? 1 : 0);
  }
}

void Export::exportSummaryOnly() {
  const ScanRecord* scan = scanHistory.getCurrentScan();
  if (!scan) return;
  EnvStats env;
  environmentAnalysis.compute(*scan, env);
  Serial.println("scanIndex,timestamp,networkCount,openCount,encryptedCount,hiddenCount,duplicateCount,bestChannel,worstChannel,avgRssi,minRssi,maxRssi");
  Serial.print(scan->scanIndex); Serial.print(',');
  Serial.print(scan->timestamp); Serial.print(',');
  Serial.print(scan->networkCount); Serial.print(',');
  Serial.print(scan->openCount); Serial.print(',');
  Serial.print(scan->encryptedCount); Serial.print(',');
  Serial.print(scan->hiddenCount); Serial.print(',');
  Serial.print(scan->duplicateCount); Serial.print(',');
  Serial.print((unsigned)env.bestChannel); Serial.print(',');
  Serial.print((unsigned)env.worstChannel); Serial.print(',');
  Serial.print(scan->avgRssi); Serial.print(',');
  Serial.print(scan->minRssi); Serial.print(',');
  Serial.println(scan->maxRssi);
}

void Export::quickReportSummary() {
  const ScanRecord* scan = scanHistory.getCurrentScan();
  if (!scan) return;
  EnvStats env;
  environmentAnalysis.compute(*scan, env);
  const NetworkRecord* safest = (scan->networkCount > 0 && env.safestNetworkIndex < scan->networkCount)
    ? &scan->networks[env.safestNetworkIndex] : nullptr;
  const NetworkRecord* riskiest = (scan->networkCount > 0 && env.riskiestNetworkIndex < scan->networkCount)
    ? &scan->networks[env.riskiestNetworkIndex] : nullptr;
  uint8_t safetyRating = 100;
  if (scan->networkCount > 0) {
    int sum = 0;
    for (uint16_t i = 0; i < scan->networkCount; i++) sum += scan->networks[i].riskScore;
    safetyRating = 100 - (uint8_t)(sum / scan->networkCount);
    if (safetyRating > 100) safetyRating = 0;
  }
  Serial.print("[WiFiGuard] #"); Serial.print(scan->scanIndex);
  Serial.print(" | Nets: "); Serial.print(scan->networkCount);
  Serial.print(" Open: "); Serial.print(scan->openCount);
  Serial.print(" Portal: "); Serial.print(scan->portalDetectedCount);
  Serial.print(" | Cong: "); Serial.print((unsigned)env.overallCongestionScore);
  Serial.print(" | Safety: "); Serial.print((unsigned)safetyRating); Serial.print("%");
  if (safest) { Serial.print(" | Safest: "); Serial.print(safest->hidden ? "?" : safest->ssid); Serial.print(" "); Serial.print(safest->riskScore); }
  if (riskiest && riskiest != safest) { Serial.print(" | Riskiest: "); Serial.print(riskiest->hidden ? "?" : riskiest->ssid); Serial.print(" "); Serial.print(riskiest->riskScore); }
  Serial.println();
}

void Export::exportHumanReadable() {
  const ScanRecord* scan = scanHistory.getCurrentScan();
  if (!scan) {
    Serial.println("[WiFiGuard] No scan data.");
    return;
  }
  EnvStats env;
  environmentAnalysis.compute(*scan, env);
  Serial.println("=== WiFiGuard Scan ===");
  Serial.print("Networks: "); Serial.print(scan->networkCount);
  Serial.print("  Open: "); Serial.print(scan->openCount);
  Serial.print("  Encrypted: "); Serial.println(scan->encryptedCount);
  Serial.print("Portals: "); Serial.print(scan->portalDetectedCount);
  Serial.print("  Duplicates: "); Serial.println(scan->duplicateCount);
  Serial.print("Best channel: "); Serial.print((unsigned)env.bestChannel);
  Serial.print("  Worst: "); Serial.print((unsigned)env.worstChannel);
  Serial.print("  Congestion: "); Serial.println((unsigned)env.overallCongestionScore);
  if (scan->networkCount > 0) {
    const NetworkRecord* s = &scan->networks[env.safestNetworkIndex];
    const NetworkRecord* r = (env.riskiestNetworkIndex < scan->networkCount) ? &scan->networks[env.riskiestNetworkIndex] : nullptr;
    Serial.print("Safest: "); Serial.print(s->hidden ? "?" : s->ssid); Serial.print(" (risk "); Serial.print(s->riskScore); Serial.println(")");
    if (r && r != s) {
      Serial.print("Riskiest: "); Serial.print(r->hidden ? "?" : r->ssid); Serial.print(" (risk "); Serial.print(r->riskScore); Serial.println(")");
    }
  }
  Serial.println("======================");
}

#if FEATURE_EXPORT_JSON
void Export::exportCurrentScanJSON(bool maskBssid) {
  bool mask = maskBssid || !settings.get().bssidInExport;
  const ScanRecord* scan = scanHistory.getCurrentScan();
  if (!scan) return;
  Serial.println("{\"scan\":{");
  Serial.print("\"index\":"); Serial.print(scan->scanIndex);
  Serial.print(",\"timestamp\":"); Serial.print(scan->timestamp);
  Serial.print(",\"networkCount\":"); Serial.println(scan->networkCount);
  Serial.println(",\"networks\":[");
  for (uint16_t i = 0; i < scan->networkCount; i++) {
    const NetworkRecord& n = scan->networks[i];
    Serial.print("{\"ssid\":\""); Serial.print(n.ssid);
    Serial.print("\",\"bssid\":\"");
    printBssid(n.bssid, mask);
    Serial.print("\",\"rssi\":"); Serial.print(n.rssi);
    Serial.print(",\"auth\":"); Serial.print((int)n.auth);
    Serial.print(",\"channel\":"); Serial.print(n.channel);
    Serial.print(",\"risk\":"); Serial.print(n.riskScore);
    Serial.print(",\"grade\":"); Serial.print((int)n.grade);
    Serial.print(",\"portal\":"); Serial.print((int)n.portalResult);
    Serial.print(",\"hidden\":"); Serial.print(n.hidden ? "true" : "false");
    Serial.print(",\"duplicate\":"); Serial.print(n.duplicateSSID ? "true" : "false");
    Serial.print("}");
    if (i < scan->networkCount - 1) Serial.print(",");
    Serial.println();
  }
  Serial.println("]}}");
}
#endif
