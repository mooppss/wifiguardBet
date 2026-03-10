/*
 * WiFiGuard - ESP32 WiFi safety and network analysis device
 * Arduino IDE target: ESP32, LilyGO with ST7789 135x240 display, 2 buttons
 *
 * Future-ready hooks (not implemented): BLE GATT export, local web dashboard
 * (AP + simple HTML), phone-assisted portal handling. See plan for extensions.
 */

#include "Config.h"
#include "Types.h"
#include "DisplayDriver.h"
#include "InputHandler.h"
#include "StateMachine.h"
#include "WiFiScanner.h"
#include "RiskEngine.h"
#include "EnvironmentAnalysis.h"
#include "ScanHistory.h"
#include "Settings.h"
#include "ConnectivityTest.h"
#include "PortalSafetyAnalyzer.h"
#include "ProtectedJoin.h"
#include "TrustedProfiles.h"
#include "Export.h"
#include "PowerManager.h"
#include "UI.h"
#include "SessionStats.h"
#include <WiFi.h>
#include <nvs_flash.h>
#include <string.h>
#if defined(ESP32)
#include <esp_task_wdt.h>
#endif

static uint32_t nextScanIndex = 1;
static DeviceState lastState = STATE_IDLE;
static uint32_t lastMonitorScan = 0;
static uint32_t demoViewRotate = 0;
static ProtectedJoinPhase lastProtectedJoinPhase = PJ_IDLE;

struct StabilityData {
  char     ssid[33];
  uint8_t  bssid[6];
  int8_t   samples[STABILITY_MAX_SAMPLES];
  uint8_t  sampleCount;
  bool     scanning;
  uint32_t lastScanTime;
  int8_t   minRssi, maxRssi;
  int32_t  sumRssi;
};
StabilityData stabilityData;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("WiFiGuard starting");

  settings.begin();
  displayDriver.begin();
  inputHandler.begin();
  stateMachine.begin();
  wifiScanner.begin();
  scanHistory.begin();
  connectivityTest.begin();
  protectedJoin.begin();
  trustedProfiles.begin();
  exportModule.begin();
  powerManager.begin();
  ui.begin();
  sessionStatsInit();
  ui.setDirty();
#if defined(ESP32)
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 30000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
#endif
}

void loop() {
#if defined(ESP32)
  esp_task_wdt_reset();
#endif
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    line.toLowerCase();
    if (line == "scan") {
      if (stateMachine.getState() == STATE_IDLE || stateMachine.getState() == STATE_BROWSING) {
        stateMachine.setState(STATE_SCANNING);
        stateMachine.setScanPhase(1);
        ui.setDirty();
      }
    } else if (line == "export") {
      if (stateMachine.getState() == STATE_BROWSING) {
        stateMachine.setState(STATE_EXPORT);
        ui.setDirty();
      } else {
        exportModule.exportCurrentScanCSV(!settings.get().bssidInExport);
      }
    } else if (line == "settings") {
      stateMachine.setState(STATE_SETTINGS);
      ui.setView(VIEW_SETTINGS);
      ui.setDirty();
    } else if (line == "summary") {
      exportModule.exportHumanReadable();
    }
  }
  InputEvent evt;
  inputHandler.update();
#if defined(ESP32) && (FEATURE_DIAG_SERIAL)
  if (settings.get().diagnosticSerial && millis() % 5000 < 30) {
    Serial.print("[Diag] state="); Serial.print((int)stateMachine.getState());
    Serial.print(" hist="); Serial.print(scanHistory.count());
    Serial.print(" heap="); Serial.println(ESP.getFreeHeap());
  }
#endif
  if (inputHandler.pollEvent(evt)) {
    powerManager.resetInactivity();
    DeviceState stBefore = stateMachine.getState();
    stateMachine.onInputEvent(evt.type);
    if (stateMachine.getState() == STATE_SCANNING && stBefore == STATE_IDLE &&
        evt.type == EVT_LONG_B1 && powerManager.isCriticalBattery()) {
      stateMachine.setState(STATE_IDLE);
      ui.setToast("Critical battery - charge first");
    }

    if (stateMachine.getState() == STATE_SLEEP) {
      displayDriver.setBacklight(settings.get().brightness);
    }

    if (stateMachine.getState() == STATE_SETTINGS) {
      ui.setView(VIEW_SETTINGS);
      ui.setDirty();
    }

    if (stateMachine.getState() == STATE_PROTECTED_JOIN) {
      // Two-button controls during join flow:
      // - R cancels and clears credentials
      // - L retries (from results) by restarting setup hotspot
      if (evt.type == EVT_TAP_B2 || evt.type == EVT_LONG_B2) {
        protectedJoin.cancel();
        stateMachine.setState(STATE_BROWSING);
        ui.setView(VIEW_LIST);
        ui.setToast("Cancelled");
        ui.setDirty();
      } else if (evt.type == EVT_TAP_B1 && protectedJoin.isInResults()) {
        protectedJoin.retry();
        ui.setDirty();
      } else if (evt.type == EVT_LONG_B1 && protectedJoin.isInResults()) {
        // Optional trusted profile (no password stored)
        ProtectedJoinStatus st = protectedJoin.getStatus();
        TrustedProfile p;
        memset(&p, 0, sizeof(p));
        strncpy(p.ssid, st.targetSsid, sizeof(p.ssid) - 1);
        p.ssid[sizeof(p.ssid) - 1] = '\0';
        strncpy(p.portalDomain, st.portalDomain, sizeof(p.portalDomain) - 1);
        p.portalDomain[sizeof(p.portalDomain) - 1] = '\0';
        p.portalSafetyMin = (uint8_t)st.portalSafety;
        if (trustedProfiles.saveProfile(p)) ui.setToast("Saved trusted profile");
        else ui.setToast("Could not save profile");
        ui.setDirty();
      }
    }

    if (stateMachine.getState() == STATE_BROWSING) {
      if (ui.getView() == VIEW_HELP && (evt.type == EVT_TAP_B1 || evt.type == EVT_TAP_B2)) {
        ui.setView(VIEW_SETTINGS);
        ui.setDirty();
      } else if (evt.type == EVT_TAP_B1) {
        if (ui.getView() == VIEW_LIST) {
          int next = ui.getListIndex() + 1;
          if (next >= ui.getFilteredCount()) next = 0;
          ui.setListIndex(next);
          ui.setListScroll(next);
        } else if (ui.getView() == VIEW_ENV_SUMMARY) {
          ui.cycleEnvPage();
        } else if (ui.getView() == VIEW_DETAIL) {
          const NetworkRecord* n = ui.getNetworkAtDisplayIndex(ui.getListIndex());
          if (n) {
            if (n->auth == AUTH_OPEN) {
              if (powerManager.isLowBattery() && settings.get().lowBatterySkipTest) {
                ui.setToast("Low battery - test skipped");
              } else {
                connectivityTest.start(n->ssid);
                stateMachine.setState(STATE_TESTING);
                stateMachine.setTestPhase(1);
              }
            } else {
              // Phone-assisted join & verify (defensive, user-authorized)
              if (powerManager.isCriticalBattery()) {
                ui.setToast("Critical battery - charge first");
              } else {
                protectedJoin.start(n->ssid);
                stateMachine.setState(STATE_PROTECTED_JOIN);
                ui.setDirty();
              }
            }
          }
        }
      } else if (evt.type == EVT_LONG_B1) {
        if (ui.getView() == VIEW_LIST && ui.getFilteredCount() > 0) {
          const NetworkRecord* n = ui.getNetworkAtDisplayIndex(ui.getListIndex());
          if (!settings.get().expertMode && n && n->portalUrl[0] && n->portalSafety == PORTAL_SUSPICIOUS)
            ui.setView(VIEW_PORTAL_SUSPICIOUS);
          else
            ui.setView(VIEW_DETAIL);
        } else if (ui.getView() == VIEW_PORTAL_SUSPICIOUS) {
          ui.setView(VIEW_DETAIL);  // Show link anyway
        } else if (ui.getView() == VIEW_DETAIL) {
          const NetworkRecord* n = ui.getNetworkAtDisplayIndex(ui.getListIndex());
          if (n && !settings.get().expertMode && n->portalUrl[0]) {
            ui.setView(VIEW_QR);  // Show QR for portal
          } else if (n) {
            memcpy(stabilityData.bssid, n->bssid, 6);
            strncpy(stabilityData.ssid, n->ssid, 32);
            stabilityData.ssid[32] = '\0';
            stabilityData.sampleCount = 0;
            stabilityData.scanning = false;
            stabilityData.lastScanTime = 0;
            stabilityData.minRssi = 0;
            stabilityData.maxRssi = -128;
            stabilityData.sumRssi = 0;
            stateMachine.setState(STATE_STABILITY_MONITOR);
            ui.setDirty();
          }
        }
      } else if (evt.type == EVT_TAP_B2) {
        if (ui.getView() == VIEW_SETTINGS) {
          int opt = ui.getSettingsOptionIndex();
          if (opt == 18) {
            ui.setView(VIEW_HELP);
            ui.setDirty();
          } else {
          SettingsRecord s = settings.get();
          if (opt == 0) {
            s.sortMode = (SortMode)(((int)s.sortMode + 1) % (int)SORT_MODE_COUNT);
          } else if (opt == 1) {
            if (s.filterFlags == 0) s.filterFlags = FILTER_HIDE_HIDDEN;
            else if (s.filterFlags == FILTER_HIDE_HIDDEN) s.filterFlags = FILTER_OPEN_ONLY;
            else if (s.filterFlags == FILTER_OPEN_ONLY) s.filterFlags = FILTER_RISKY_ONLY;
            else if (s.filterFlags == FILTER_RISKY_ONLY) s.filterFlags = FILTER_HIDE_LOW_SIGNAL;
            else s.filterFlags = 0;
          } else if (opt == 2) s.expertMode = !s.expertMode;
          else if (opt == 3) s.diagnosticSerial = !s.diagnosticSerial;
          else if (opt == 4) s.lowPowerMode = !s.lowPowerMode;
          else if (opt == 5) s.alwaysMonitor = !s.alwaysMonitor;
          else if (opt == 6) s.alertOnHighRisk = !s.alertOnHighRisk;
          else if (opt == 7) s.exportSummaryOnly = !s.exportSummaryOnly;
          else if (opt == 8) s.demoMode = !s.demoMode;
          else if (opt == 9) {
            s.privacyMode = !s.privacyMode;
            if (s.privacyMode) { s.sessionOnly = true; s.bssidInExport = false; s.expertMode = false; }
          } else if (opt == 10) {
            if (s.brightness <= 25) s.brightness = 50;
            else if (s.brightness <= 50) s.brightness = 75;
            else if (s.brightness <= 75) s.brightness = 100;
            else s.brightness = 25;
            displayDriver.setBacklight(s.brightness);
          } else if (opt == 11) {
            if (s.inactivityMs <= 60000) s.inactivityMs = 120000;
            else if (s.inactivityMs <= 120000) s.inactivityMs = 300000;
            else if (s.inactivityMs <= 300000) s.inactivityMs = 600000;
            else s.inactivityMs = 60000;
          } else if (opt == 12) {
            if (s.monitorIntervalSec <= 30) s.monitorIntervalSec = 60;
            else if (s.monitorIntervalSec <= 60) s.monitorIntervalSec = 120;
            else s.monitorIntervalSec = 30;
          } else if (opt == 13) {
            if (s.stabilityMonitorIntervalSec <= 10) s.stabilityMonitorIntervalSec = 15;
            else if (s.stabilityMonitorIntervalSec <= 15) s.stabilityMonitorIntervalSec = 30;
            else s.stabilityMonitorIntervalSec = 10;
          } else if (opt == 14) s.lowBatterySkipTest = !s.lowBatterySkipTest;
          else if (opt == 15) s.lowBatteryReduceMonitor = !s.lowBatteryReduceMonitor;
          else if (opt == 16) {
            if (s.lowBatteryDimPct <= 20) s.lowBatteryDimPct = 30;
            else if (s.lowBatteryDimPct <= 30) s.lowBatteryDimPct = 50;
            else s.lowBatteryDimPct = 20;
          } else if (opt == 17) s.highContrast = !s.highContrast;
          settings.save(s);
          ui.setDirty();
          }
        } else if (ui.getView() == VIEW_PORTAL_SUSPICIOUS || ui.getView() == VIEW_QR) {
          if (ui.getView() == VIEW_QR)
            ui.setView(VIEW_DETAIL);
          else
            ui.setView(VIEW_LIST);
        } else if (ui.getView() == VIEW_DETAIL || ui.getView() == VIEW_ENV_SUMMARY) {
          ui.setView(VIEW_LIST);
        } else if (ui.getView() == VIEW_LIST) {
          if (ui.hasHighRiskAlert()) ui.setHighRiskAlert(false);
          else {
            int prev = ui.getListIndex() - 1;
            if (prev < 0) prev = ui.getFilteredCount() > 0 ? ui.getFilteredCount() - 1 : 0;
            ui.setListIndex(prev);
            ui.setListScroll(prev);
          }
        }
      } else if (evt.type == EVT_LONG_B2) {
        if (ui.getView() == VIEW_LIST) {
          ui.setView(VIEW_ENV_SUMMARY);
        } else if (ui.getView() == VIEW_ENV_SUMMARY) {
          ui.setView(VIEW_SETTINGS);
        } else if (ui.getView() == VIEW_SETTINGS) {
          stateMachine.onSettingsExit();
          ui.setView(VIEW_LIST);
        }
      }
      if (ui.getView() == VIEW_SETTINGS && evt.type == EVT_TAP_B1) {
        int next = (ui.getSettingsOptionIndex() + 1) % 19;
        ui.setSettingsOptionIndex(next);
      }
    }
    if (stateMachine.getState() == STATE_EXPORT) {
      if (evt.type == EVT_TAP_B1) stateMachine.onExportComplete();
    }
    if (stateMachine.getState() == STATE_SETTINGS) {
      if (ui.getView() == VIEW_HELP) {
        if (evt.type == EVT_TAP_B1 || evt.type == EVT_TAP_B2) {
          ui.setView(VIEW_SETTINGS);
          ui.setDirty();
        }
      } else if (evt.type == EVT_TAP_B1) {
        int next = (ui.getSettingsOptionIndex() + 1) % 19;
        ui.setSettingsOptionIndex(next);
      } else if (evt.type == EVT_TAP_B2) {
        int opt = ui.getSettingsOptionIndex();
        if (opt == 18) {
          ui.setView(VIEW_HELP);
          ui.setDirty();
        } else {
        SettingsRecord s = settings.get();
        if (opt == 0) s.sortMode = (SortMode)(((int)s.sortMode + 1) % (int)SORT_MODE_COUNT);
        else if (opt == 1) {
          if (s.filterFlags == 0) s.filterFlags = FILTER_HIDE_HIDDEN;
          else if (s.filterFlags == FILTER_HIDE_HIDDEN) s.filterFlags = FILTER_OPEN_ONLY;
          else if (s.filterFlags == FILTER_OPEN_ONLY) s.filterFlags = FILTER_RISKY_ONLY;
          else if (s.filterFlags == FILTER_RISKY_ONLY) s.filterFlags = FILTER_HIDE_LOW_SIGNAL;
          else s.filterFlags = 0;
        }
        else if (opt == 2) s.expertMode = !s.expertMode;
        else if (opt == 3) s.diagnosticSerial = !s.diagnosticSerial;
        else if (opt == 4) s.lowPowerMode = !s.lowPowerMode;
        else if (opt == 5) s.alwaysMonitor = !s.alwaysMonitor;
        else if (opt == 6) s.alertOnHighRisk = !s.alertOnHighRisk;
        else if (opt == 7) s.exportSummaryOnly = !s.exportSummaryOnly;
        else if (opt == 8) s.demoMode = !s.demoMode;
        else if (opt == 9) {
          s.privacyMode = !s.privacyMode;
          if (s.privacyMode) { s.sessionOnly = true; s.bssidInExport = false; s.expertMode = false; }
        }
        else if (opt == 10) {
          if (s.brightness <= 25) s.brightness = 50;
          else if (s.brightness <= 50) s.brightness = 75;
          else if (s.brightness <= 75) s.brightness = 100;
          else s.brightness = 25;
          displayDriver.setBacklight(s.brightness);
        } else if (opt == 11) {
          if (s.inactivityMs <= 60000) s.inactivityMs = 120000;
          else if (s.inactivityMs <= 120000) s.inactivityMs = 300000;
          else if (s.inactivityMs <= 300000) s.inactivityMs = 600000;
          else s.inactivityMs = 60000;
        } else if (opt == 12) {
          if (s.monitorIntervalSec <= 30) s.monitorIntervalSec = 60;
          else if (s.monitorIntervalSec <= 60) s.monitorIntervalSec = 120;
          else s.monitorIntervalSec = 30;
        } else if (opt == 13) {
          if (s.stabilityMonitorIntervalSec <= 10) s.stabilityMonitorIntervalSec = 15;
          else if (s.stabilityMonitorIntervalSec <= 15) s.stabilityMonitorIntervalSec = 30;
          else s.stabilityMonitorIntervalSec = 10;
        } else if (opt == 14) s.lowBatterySkipTest = !s.lowBatterySkipTest;
        else if (opt == 15) s.lowBatteryReduceMonitor = !s.lowBatteryReduceMonitor;
        else if (opt == 16) {
          if (s.lowBatteryDimPct <= 20) s.lowBatteryDimPct = 30;
          else if (s.lowBatteryDimPct <= 30) s.lowBatteryDimPct = 50;
          else s.lowBatteryDimPct = 20;
        } else if (opt == 17) s.highContrast = !s.highContrast;
        settings.save(s);
        ui.setDirty();
        }
      } else if (evt.type == EVT_LONG_B2 && lastState == STATE_SETTINGS) {
        stateMachine.onSettingsExit();
        ui.setDirty();
      } else if (evt.type == EVT_CHORD) {
        stateMachine.setState(STATE_DEBUG);
        ui.setDirty();
      }
    }
    if (stateMachine.getState() == STATE_STABILITY_MONITOR) {
      if (evt.type == EVT_TAP_B2 || evt.type == EVT_LONG_B2) {
        WiFi.scanDelete();
        stabilityData.scanning = false;
        stateMachine.setState(STATE_BROWSING);
        ui.setView(VIEW_DETAIL);
        ui.setDirty();
      }
    }
    if (stateMachine.getState() == STATE_DEBUG) {
      if (evt.type == EVT_TAP_B2) {
        stateMachine.setState(STATE_IDLE);
        ui.setDirty();
      } else if (evt.type == EVT_LONG_B2) {
        nvs_flash_erase();
        ESP.restart();
      }
    }
  }

  if (stateMachine.getState() == STATE_SCANNING && stateMachine.getScanPhase() == 1) {
    wifiScanner.startScan();
    stateMachine.setScanPhase(0);
    ui.setHighRiskAlert(false);
  }

  // Animate scanning and testing screens
  if (stateMachine.getState() == STATE_SCANNING || stateMachine.getState() == STATE_TESTING) {
    static uint32_t lastAnimRefresh = 0;
    if (millis() - lastAnimRefresh >= 400) {
      lastAnimRefresh = millis();
      ui.setDirty();
    }
  }

  if (stateMachine.getState() == STATE_IDLE && settings.get().alwaysMonitor) {
    uint32_t intervalMs = (uint32_t)settings.get().monitorIntervalSec * 1000;
    if (powerManager.isLowBattery() && settings.get().lowBatteryReduceMonitor) intervalMs *= 2;
    if (intervalMs > 0 && (millis() - lastMonitorScan) >= intervalMs) {
      lastMonitorScan = millis();
      stateMachine.setState(STATE_SCANNING);
      stateMachine.setScanPhase(1);
    }
  }

  if (stateMachine.getState() == STATE_SCANNING) {
    if (wifiScanner.update()) {
      static ScanRecord scan;
      wifiScanner.getResult(scan);
      scan.scanIndex = nextScanIndex++;
      const ScanRecord* prev = scanHistory.getLatestPtr();
      if (prev) {
        wifiScanner.markNewNetworks(scan, prev);
        wifiScanner.fillRssiFromPrevious(scan, prev);
        scanHistory.markGoneInPreviousScan(scan);
      }
      EnvStats env;
      environmentAnalysis.compute(scan, env);
      uint8_t channelCong[WIFI_CHANNELS + 1];
      for (int i = 0; i <= WIFI_CHANNELS; i++) channelCong[i] = env.congestionPerChannel[i];
      riskEngine.compute(scan, channelCong);
      scan.portalDetectedCount = env.portalDetectedCount;
      scan.safestNetworkIndex = env.safestNetworkIndex;
      scan.riskiestNetworkIndex = env.riskiestNetworkIndex;
      sessionStatsOnScan(scan);
      scanHistory.setCurrentScan(scan);
      scanHistory.push(scan);
      scanHistory.markCurrentStored();
      lastMonitorScan = millis();
      uint16_t newCount = 0;
      for (uint16_t i = 0; i < scan.networkCount; i++) {
        if (scan.networks[i].newThisScan) newCount++;
      }
      if (newCount >= ANOMALY_NEW_THRESHOLD && prev) {
        ui.setToast("Anomaly: many new APs!");
        Serial.print("[Alert] Anomaly: ");
        Serial.print(newCount);
        Serial.println(" new networks appeared");
      }
      if (settings.get().alertOnHighRisk) {
        for (uint16_t i = 0; i < scan.networkCount; i++) {
          if (scan.networks[i].auth == AUTH_OPEN && scan.networks[i].riskScore >= 70) {
            ui.setHighRiskAlert(true);
            break;
          }
        }
      }
      stateMachine.onScanComplete();
      stateMachine.onProcessingComplete();
      powerManager.resetInactivity();
      ui.setToast("Scan complete");
      ui.setView(VIEW_LIST);
      ui.setListIndex(0);
      ui.setListScroll(0);
      if (settings.get().demoMode) demoViewRotate = millis();
      ui.setDirty();
    }
  }

  if (stateMachine.getState() == STATE_BROWSING && settings.get().demoMode) {
    if ((millis() - demoViewRotate) >= 8000) {
      demoViewRotate = millis();
      if (ui.getView() == VIEW_LIST) ui.setView(VIEW_ENV_SUMMARY);
      else ui.setView(VIEW_LIST);
    }
  }

  if (stateMachine.getState() == STATE_TESTING) {
    if (connectivityTest.update()) {
      ConnectivityResult res;
      connectivityTest.getResult(res);
      const char* testSsid = connectivityTest.getTestedSsid();
      ScanRecord* cur = scanHistory.getCurrentScanForUpdate();
      if (cur && testSsid) {
        for (uint16_t i = 0; i < cur->networkCount; i++) {
          if (strcmp(cur->networks[i].ssid, testSsid) == 0) {
            scanHistory.updateNetworkConnectivity(i, res.grade, res.portal);
            if (res.benchmarkPings > 0)
              scanHistory.updateNetworkBenchmark(i, res.benchmarkAvgMs, res.benchmarkJitterMs);
            if (res.portalUrl[0] && res.portal == PORTAL_REDIRECT_LOGIN) {
              portalSafetyAnalyzer.evaluatePortalSafety(res.portalUrl, cur->networks[i].ssid, &cur->networks[i]);
            } else {
              cur->networks[i].portalUrl[0] = '\0';
              cur->networks[i].portalDomain[0] = '\0';
              cur->networks[i].portalIsIP = false;
              cur->networks[i].portalIsHTTPS = false;
              cur->networks[i].portalIsLocalGateway = false;
              cur->networks[i].portalPathLooksLikePortal = false;
              cur->networks[i].portalBrandMismatch = false;
              cur->networks[i].portalLongUrl = false;
              cur->networks[i].portalSafetyScore = 0;
              cur->networks[i].portalSafety = PORTAL_SAFE;
            }
            riskEngine.computeOne(cur->networks[i], nullptr);
            break;
          }
        }
      }
      inputHandler.flushQueue();
      stateMachine.onTestComplete();
      ui.setDirty();
    }
  }

  if (stateMachine.getState() == STATE_PROTECTED_JOIN) {
    protectedJoin.update();
    ProtectedJoinStatus pj = protectedJoin.getStatus();
    if (pj.phase != lastProtectedJoinPhase) {
      ui.setDirty();
      lastProtectedJoinPhase = pj.phase;
    }
  } else {
    lastProtectedJoinPhase = PJ_IDLE;
  }

  if (stateMachine.getState() == STATE_EXPORT && lastState != STATE_EXPORT) {
    if (settings.get().exportSummaryOnly)
      exportModule.quickReportSummary();
    else
      exportModule.exportCurrentScanCSV(!settings.get().bssidInExport);
  }
  lastState = stateMachine.getState();

  if (stateMachine.getState() == STATE_STABILITY_MONITOR) {
    uint32_t interval = (uint32_t)settings.get().stabilityMonitorIntervalSec * 1000;
    if (interval < 5000) interval = 5000;
    if (!stabilityData.scanning) {
      if (millis() - stabilityData.lastScanTime >= interval) {
        WiFi.scanNetworks(true);
        stabilityData.scanning = true;
      }
    } else {
      int n = WiFi.scanComplete();
      if (n >= 0) {
        bool found = false;
        for (int i = 0; i < n; i++) {
          if (memcmp(WiFi.BSSID(i), stabilityData.bssid, 6) == 0) {
            int8_t rssi = (int8_t)WiFi.RSSI(i);
            if (stabilityData.sampleCount < STABILITY_MAX_SAMPLES) {
              stabilityData.samples[stabilityData.sampleCount] = rssi;
              stabilityData.sampleCount++;
            } else {
              stabilityData.sumRssi -= stabilityData.samples[0];
              for (int s = 0; s < STABILITY_MAX_SAMPLES - 1; s++)
                stabilityData.samples[s] = stabilityData.samples[s + 1];
              stabilityData.samples[STABILITY_MAX_SAMPLES - 1] = rssi;
            }
            stabilityData.sumRssi += rssi;
            if (rssi < stabilityData.minRssi || stabilityData.minRssi == 0) stabilityData.minRssi = rssi;
            if (rssi > stabilityData.maxRssi) stabilityData.maxRssi = rssi;
            found = true;
            break;
          }
        }
        if (!found && stabilityData.sampleCount > 0) {
          ui.setToast("Target AP not found");
        }
        WiFi.scanDelete();
        stabilityData.scanning = false;
        stabilityData.lastScanTime = millis();
        ui.setDirty();
      }
    }
  }

  if (stateMachine.getState() == STATE_DEBUG) {
    static uint32_t lastDebugRefresh = 0;
    if (millis() - lastDebugRefresh >= 1000) {
      lastDebugRefresh = millis();
      ui.setDirty();
    }
  }

  powerManager.update();
  ui.draw();

  delay(5);
}
