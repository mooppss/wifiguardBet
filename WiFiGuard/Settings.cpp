#include "Settings.h"
#include <Preferences.h>

Settings settings;
static Preferences prefs;

void Settings::begin() {
  prefs.begin(NVS_NAMESPACE, true);
  load(data_);
  prefs.end();
}

void Settings::load(SettingsRecord& out) const {
  out.historySize = prefs.getUChar("histSize", HISTORY_SIZE);
  out.sessionOnly = prefs.getBool("sessOnly", (bool)SESSION_ONLY_DEFAULT);
  out.bssidInExport = prefs.getBool("bssidExp", true);
  out.inactivityMs = prefs.getULong("inactMs", INACTIVITY_MS);
  out.brightness = prefs.getUChar("bright", 100);
  out.sortMode = (SortMode)prefs.getUChar("sort", (uint8_t)SORT_RSSI);
  out.filterFlags = prefs.getUChar("filter", FILTER_NONE);
  out.connectTimeoutMs = prefs.getUShort("connMs", CONNECT_TIMEOUT_MS);
  out.dnsTimeoutMs = prefs.getUShort("dnsMs", DNS_TIMEOUT_MS);
  out.httpTimeoutMs = prefs.getUShort("httpMs", HTTP_TIMEOUT_MS);
  out.lowBatteryPct = prefs.getUChar("lowBat", LOW_BATTERY_PCT);
  out.expertMode = prefs.getBool("expert", false);
  out.diagnosticSerial = prefs.getBool("diag", false);
  out.lowPowerMode = prefs.getBool("lowPwr", false);
  out.alwaysMonitor = prefs.getBool("monitor", false);
  out.monitorIntervalSec = prefs.getUShort("monInt", 60);
  out.alertOnHighRisk = prefs.getBool("alert", false);
  out.exportSummaryOnly = prefs.getBool("expSum", false);
  out.demoMode = prefs.getBool("demo", false);
  out.stabilityMonitorIntervalSec = prefs.getUShort("stabInt", 15);
  out.lowBatteryDimPct = prefs.getUChar("lowDim", 30);
  out.lowBatterySkipTest = prefs.getBool("lowSkip", true);
  out.lowBatteryReduceMonitor = prefs.getBool("lowMon", true);
  out.privacyMode = prefs.getBool("privacy", false);
  out.highContrast = prefs.getBool("hc", false);
}

void Settings::save(const SettingsRecord& in) {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putUChar("histSize", in.historySize);
  prefs.putBool("sessOnly", in.sessionOnly);
  prefs.putBool("bssidExp", in.bssidInExport);
  prefs.putULong("inactMs", in.inactivityMs);
  prefs.putUChar("bright", in.brightness);
  prefs.putUChar("sort", (uint8_t)in.sortMode);
  prefs.putUChar("filter", in.filterFlags);
  prefs.putUShort("connMs", in.connectTimeoutMs);
  prefs.putUShort("dnsMs", in.dnsTimeoutMs);
  prefs.putUShort("httpMs", in.httpTimeoutMs);
  prefs.putUChar("lowBat", in.lowBatteryPct);
  prefs.putBool("expert", in.expertMode);
  prefs.putBool("diag", in.diagnosticSerial);
  prefs.putBool("lowPwr", in.lowPowerMode);
  prefs.putBool("monitor", in.alwaysMonitor);
  prefs.putUShort("monInt", in.monitorIntervalSec);
  prefs.putBool("alert", in.alertOnHighRisk);
  prefs.putBool("expSum", in.exportSummaryOnly);
  prefs.putBool("demo", in.demoMode);
  prefs.putUShort("stabInt", in.stabilityMonitorIntervalSec);
  prefs.putUChar("lowDim", in.lowBatteryDimPct);
  prefs.putBool("lowSkip", in.lowBatterySkipTest);
  prefs.putBool("lowMon", in.lowBatteryReduceMonitor);
  prefs.putBool("privacy", in.privacyMode);
  prefs.putBool("hc", in.highContrast);
  prefs.end();
  data_ = in;
}
