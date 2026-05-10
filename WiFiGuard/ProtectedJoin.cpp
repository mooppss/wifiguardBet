#include "ProtectedJoin.h"
#include "Settings.h"
#include "PortalSafetyAnalyzer.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Arduino.h>
#include <string.h>

// SoftAP + local web config
#include <WebServer.h>
#include <DNSServer.h>

ProtectedJoin protectedJoin;

static WebServer g_server(80);
static DNSServer g_dns;

static void safeCopy(char* dst, int dstSize, const char* src) {
  if (!dst || dstSize <= 0) return;
  if (!src) { dst[0] = '\0'; return; }
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

static uint32_t rand32_() {
#if defined(ESP32)
  return (uint32_t)esp_random();
#else
  return (uint32_t)random(0x7fffffff);
#endif
}

static char randAlnum_() {
  const char* a = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  return a[rand32_() % 32];
}

void ProtectedJoin::begin() {
  active_ = false;
  hasPassword_ = false;
  failureCount_ = 0;
  checkPhase_ = 0;
  checkStartMs_ = 0;
  phaseStartMs_ = 0;
  apSsid_[0] = apPass_[0] = pairingCode_[0] = '\0';
  password_[0] = '\0';
  portalUrl_[0] = '\0';

  memset(&status_, 0, sizeof(status_));
  status_.phase = PJ_IDLE;
  status_.verdict = PJV_COULD_NOT_VERIFY;
  status_.portalSafety = PORTAL_CAUTION;
  status_.portalSafetyScore = 0;
  status_.benchmarkAvgMs = -1;
  status_.benchmarkJitterMs = -1;
  status_.benchmarkPings = 0;
}

void ProtectedJoin::clearSensitive_() {
  // RAM-only clearing. (Best-effort; compiler may optimize in some builds.)
  for (size_t i = 0; i < sizeof(password_); i++) password_[i] = 0;
  hasPassword_ = false;
}

void ProtectedJoin::start(const char* targetSsid) {
  Serial.print("[PJ] start() for SSID: "); Serial.println(targetSsid ? targetSsid : "(null)");
  cancel();
  active_ = true;
  failureCount_ = 0;
  safeCopy(status_.targetSsid, sizeof(status_.targetSsid), targetSsid);
  status_.phase = PJ_SETUP_AP;
  status_.progressStep = 0;
  status_.verdict = PJV_COULD_NOT_VERIFY;
  status_.hasPortalUrl = false;
  status_.portalDomain[0] = '\0';
  status_.portalSafety = PORTAL_CAUTION;
  status_.portalSafetyScore = 0;
  status_.benchmarkAvgMs = -1;
  status_.benchmarkJitterMs = -1;
  status_.benchmarkPings = 0;
  phaseStartMs_ = millis();
  startSoftAp_();
  startWebServer_();
  status_.phase = PJ_WAITING_FOR_PHONE;
}

void ProtectedJoin::cancel() {
  Serial.println("[PJ] cancel() — restoring STA mode.");
  stopWebServer_();
  stopSoftAp_();
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  clearSensitive_();
  active_ = false;
  status_.phase = PJ_IDLE;
}

void ProtectedJoin::retry() {
  if (!active_) return;
  // Rate limit: after multiple failures, always require phone re-entry (no cached password).
  if (failureCount_ >= 3) failureCount_ = 0;
  clearSensitive_();
  stopWebServer_();
  stopSoftAp_();
  WiFi.disconnect(true);
  status_.phase = PJ_SETUP_AP;
  status_.progressStep = 0;
  status_.verdict = PJV_COULD_NOT_VERIFY;
  status_.hasPortalUrl = false;
  status_.portalDomain[0] = '\0';
  status_.portalSafety = PORTAL_CAUTION;
  status_.portalSafetyScore = 0;
  status_.benchmarkAvgMs = -1;
  status_.benchmarkJitterMs = -1;
  status_.benchmarkPings = 0;
  phaseStartMs_ = millis();
  startSoftAp_();
  startWebServer_();
  status_.phase = PJ_WAITING_FOR_PHONE;
}

bool ProtectedJoin::validatePairing_(const char* code) const {
  if (!code) return false;
  return strcmp(code, pairingCode_) == 0;
}

void ProtectedJoin::buildHtml_(char* out, int outSize) const {
  // Minimal browser-only UX (consent + pairing + password). SSID is locked to selected network.
  // No external assets; single page.
  const char* ssid = status_.targetSsid[0] ? status_.targetSsid : "";
  snprintf(out, outSize,
    "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>WiFiGuard Secure Join</title>"
    "<style>body{font-family:sans-serif;margin:16px;max-width:520px}"
    "h1{font-size:18px}label{display:block;margin-top:12px}"
    "input{width:100%%;padding:10px;font-size:16px}button{margin-top:16px;padding:12px;width:100%%;font-size:16px}"
    ".note{color:#555;font-size:13px;margin-top:10px}</style></head><body>"
    "<h1>WiFiGuard: Authorized Secure Join</h1>"
    "<div class='note'>Only enter credentials for a Wi‑Fi network you are authorized to use. "
    "WiFiGuard does not break into networks; it verifies safety after you legitimately connect.</div>"
    "<form method='POST' action='/submit'>"
    "<label>Pairing code (shown on WiFiGuard)</label><input name='code' inputmode='numeric' maxlength='6' required>"
    "<label>Network (SSID)</label><input name='ssid' value='%s' readonly>"
    "<label>Password</label><input name='pw' type='password' maxlength='63' required>"
    "<button type='submit'>Send to WiFiGuard</button>"
    "</form></body></html>", ssid);
}

void ProtectedJoin::startSoftAp_() {
  // Generate per-session AP SSID/password and pairing code
  uint16_t suf = (uint16_t)(rand32_() & 0xFFFF);
  snprintf(apSsid_, sizeof(apSsid_), "WiFiGuard-Setup-%04X", (unsigned)suf);
  for (int i = 0; i < 10 && i < (int)sizeof(apPass_) - 1; i++) apPass_[i] = randAlnum_();
  apPass_[10] = '\0';
  snprintf(pairingCode_, sizeof(pairingCode_), "%02u%02u",
           (unsigned)(rand32_() % 100), (unsigned)(rand32_() % 100));

  Serial.println("[PJ] Starting setup AP...");
  Serial.print("[PJ] AP SSID: "); Serial.println(apSsid_);
  Serial.print("[PJ] AP Pass: "); Serial.println(apPass_);
  Serial.print("[PJ] Pairing code: "); Serial.println(pairingCode_);

  // Prevent the ESP32 from writing/restoring STA credentials from flash and from
  // silently trying to reconnect in the background. Both would compete with the AP.
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);

  // WIFI_OFF is the only way to fully stop the esp-idf WiFi stack and clear all
  // internal driver state. WiFi.disconnect(true) is not equivalent — it only
  // disconnects the STA but leaves the radio running, and residual STA state
  // can prevent softAP from initialising cleanly on the next mode switch.
  WiFi.mode(WIFI_OFF);
  delay(200);

  // Use pure AP mode (not AP_STA) while waiting for the phone.
  // On a single-radio ESP32, AP_STA forces channel-sharing between AP and STA;
  // that causes the AP beacon to drop whenever the radio hops channels for STA
  // background activity — making the SSID invisible to phones intermittently.
  WiFi.mode(WIFI_AP);
  delay(100);

  // Explicit static IP config initialises the AP's DHCP server before softAP().
  // Without this, the DHCP server can start in a bad state: phones associate at
  // the radio layer but never receive an IP, so the network appears non-existent.
  WiFi.softAPConfig(
    IPAddress(192, 168, 4, 1),   // AP IP
    IPAddress(192, 168, 4, 1),   // gateway
    IPAddress(255, 255, 255, 0)  // subnet
  );

  // Channel 6 is one of the three non-overlapping 2.4 GHz channels (1, 6, 11).
  // Explicit value prevents the radio from reusing whatever channel was active
  // during the last scan. max_conn=4, hidden=0 (SSID must be broadcast).
  bool ok = WiFi.softAP(apSsid_, apPass_, /*channel=*/6, /*hidden=*/0, /*max_conn=*/4);

  // Allow the beacon to start transmitting before we hand IP address to DNS.
  delay(500);

  if (!ok) {
    Serial.println("[PJ] ERROR: WiFi.softAP() failed — AP will not be visible!");
  } else {
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("[PJ] AP started OK on ch6. IP: "); Serial.println(apIP);
    Serial.print("[PJ] AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  }

  // Captive DNS: redirect all DNS queries to our IP so browsers auto-open the portal.
  IPAddress apIP = WiFi.softAPIP();
  g_dns.start(53, "*", apIP);
  Serial.print("[PJ] DNS captive portal running on "); Serial.println(apIP);
}

void ProtectedJoin::stopSoftAp_() {
  Serial.println("[PJ] Stopping AP and DNS.");
  g_dns.stop();
  WiFi.softAPdisconnect(true);
  delay(50);
}

void ProtectedJoin::startWebServer_() {
  g_server.stop();
  g_server.on("/", HTTP_GET, [this]() {
    Serial.println("[PJ] Phone loaded setup page (GET /).");
    static char html[1100];
    buildHtml_(html, sizeof(html));
    g_server.send(200, "text/html", html);
  });

  g_server.on("/submit", HTTP_POST, [this]() {
    const char* code = g_server.arg("code").c_str();
    const char* ssid = g_server.arg("ssid").c_str();
    const char* pw = g_server.arg("pw").c_str();

    Serial.println("[PJ] /submit received from phone.");
    if (!validatePairing_(code)) {
      Serial.println("[PJ] /submit: pairing code mismatch.");
      g_server.send(403, "text/plain", "Pairing code incorrect.");
      return;
    }
    // Lock SSID to the selected target in MVP.
    if (strcmp(ssid, status_.targetSsid) != 0) {
      g_server.send(400, "text/plain", "SSID mismatch.");
      return;
    }
    if (!pw || strlen(pw) < 1 || strlen(pw) > 63) {
      g_server.send(400, "text/plain", "Password length invalid.");
      return;
    }

    safeCopy(password_, sizeof(password_), pw);
    hasPassword_ = true;
    g_server.send(200, "text/plain", "Received. You can return to WiFiGuard.");

    // Transition to connecting on next update cycle.
    status_.phase = PJ_CONNECTING;
    phaseStartMs_ = millis();
    status_.progressStep = 1;
    stopWebServer_();
    stopSoftAp_();
    beginConnect_();
  });

  g_server.begin();
}

void ProtectedJoin::stopWebServer_() {
  g_server.stop();
}

void ProtectedJoin::beginConnect_() {
  status_.connected = false;
  status_.dhcpOk = false;
  status_.dnsOk = false;
  status_.httpOk = false;
  Serial.print("[PJ] Connecting to: "); Serial.println(status_.targetSsid);
  // AP is already stopped by this point (stopSoftAp_() called in /submit handler).
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);
  if (status_.targetSsid[0] && hasPassword_)
    WiFi.begin(status_.targetSsid, password_);
}

void ProtectedJoin::updateConnect_() {
  uint32_t connMs = settings.get().connectTimeoutMs;
  if (WiFi.status() == WL_CONNECTED) {
    status_.connected = true;
    // DHCP/IP check
    IPAddress ip = WiFi.localIP();
    status_.dhcpOk = (ip[0] != 0);
    status_.phase = PJ_CHECKING;
    status_.progressStep = 2;
    beginChecks_();
    return;
  }
  if (millis() - phaseStartMs_ > connMs) {
    failureCount_++;
    // Misuse prevention: prevent rapid repeated attempts without user intent.
    // After 3 failures, require re-entry via phone (retry restarts setup).
    status_.phase = PJ_RESULTS;
    status_.verdict = PJV_COULD_NOT_VERIFY;
    status_.progressStep = 0;
    finalizeVerdict_();
    return;
  }
}

void ProtectedJoin::beginChecks_() {
  checkPhase_ = 1;
  checkStartMs_ = millis();
  status_.dnsOk = false;
  status_.httpOk = false;
  status_.hasPortalUrl = false;
  status_.portalDomain[0] = '\0';
  status_.portalSafety = PORTAL_CAUTION;
  status_.portalSafetyScore = 0;
  portalUrl_[0] = '\0';
  status_.benchmarkAvgMs = -1;
  status_.benchmarkJitterMs = -1;
  status_.benchmarkPings = 0;
}

void ProtectedJoin::updateChecks_() {
  uint32_t dnsMs = settings.get().dnsTimeoutMs;
  uint32_t httpMs = settings.get().httpTimeoutMs;

  if (checkPhase_ == 1) {
    WiFiClient client;
    if (!client.connect("connectivitycheck.gstatic.com", 80, (int)dnsMs)) {
      status_.dnsOk = false;
      checkPhase_ = 99;
      return;
    }
    status_.dnsOk = true;
    client.stop();
    checkPhase_ = 2;
    checkStartMs_ = millis();
    return;
  }

  if (checkPhase_ == 2) {
    HTTPClient http;
    http.setTimeout(httpMs / 1000);
    http.begin(CONNECTIVITY_URL);
    int code = http.GET();
    bool ok = (code == 204 || (code >= 200 && code < 300));
    bool redirect = (code == 301 || code == 302);
    status_.httpOk = (code > 0);
    if (redirect) {
      String loc = http.getLocation();
      if (loc.length() > 0) {
        safeCopy(portalUrl_, sizeof(portalUrl_), loc.c_str());
        status_.hasPortalUrl = true;
      }
    }
    http.end();

    if (ok) {
      // Benchmark (simple)
      int times[BENCHMARK_PINGS];
      int minT = 30000, maxT = 0, sum = 0, count = 0;
      for (int i = 0; i < BENCHMARK_PINGS; i++) {
        HTTPClient b;
        b.setTimeout(3);
        uint32_t st = millis();
        b.begin(CONNECTIVITY_URL);
        int bc = b.GET();
        int elapsed = (int)(millis() - st);
        b.end();
        times[i] = (bc > 0) ? elapsed : -1;
      }
      for (int i = 0; i < BENCHMARK_PINGS; i++) {
        if (times[i] > 0) {
          sum += times[i];
          count++;
          if (times[i] < minT) minT = times[i];
          if (times[i] > maxT) maxT = times[i];
        }
      }
      if (count > 0) {
        status_.benchmarkAvgMs = (int16_t)(sum / count);
        status_.benchmarkJitterMs = (int16_t)(maxT - minT);
        status_.benchmarkPings = (uint8_t)count;
      }
      checkPhase_ = 3;
      return;
    }

    // If redirect, treat as portal
    if (redirect) {
      checkPhase_ = 3;
      return;
    }

    // Fallback endpoint (matches existing connectivity logic)
    HTTPClient http2;
    http2.setTimeout(httpMs / 1000);
    http2.begin(CONNECTIVITY_URL_ALT);
    int code2 = http2.GET();
    bool ok2 = (code2 == 204 || (code2 >= 200 && code2 < 300));
    bool redirect2 = (code2 == 301 || code2 == 302);
    if (redirect2) {
      String loc = http2.getLocation();
      if (loc.length() > 0) {
        safeCopy(portalUrl_, sizeof(portalUrl_), loc.c_str());
        status_.hasPortalUrl = true;
      }
    }
    http2.end();

    status_.httpOk = status_.httpOk || (code2 > 0);
    if (ok2 || redirect2) {
      checkPhase_ = 3;
      return;
    }

    checkPhase_ = 99;
    return;
  }

  if (checkPhase_ == 3) {
    // Portal analysis if needed
    if (status_.hasPortalUrl && portalUrl_[0]) {
      NetworkRecord tmp;
      memset(&tmp, 0, sizeof(tmp));
      safeCopy(tmp.ssid, sizeof(tmp.ssid), status_.targetSsid);
      portalSafetyAnalyzer.evaluatePortalSafety(portalUrl_, tmp.ssid, &tmp);
      safeCopy(status_.portalDomain, sizeof(status_.portalDomain), tmp.portalDomain);
      status_.portalSafety = tmp.portalSafety;
      status_.portalSafetyScore = tmp.portalSafetyScore;
    }
    checkPhase_ = 100;
    return;
  }
}

void ProtectedJoin::finalizeVerdict_() {
  // Credentials must be cleared automatically unless user explicitly opts in to save a profile.
  // MVP: no password storage, so we clear now.
  clearSensitive_();

  // Default mapping (simple, defensive)
  if (!status_.connected || !status_.dhcpOk) {
    status_.verdict = PJV_COULD_NOT_VERIFY;
    return;
  }
  if (status_.hasPortalUrl) {
    status_.verdict = PJV_LOGIN_REQUIRED;
    if (status_.portalSafety == PORTAL_SUSPICIOUS) status_.verdict = PJV_SUSPICIOUS_BEHAVIOR;
    else if (status_.portalSafety == PORTAL_CAUTION) status_.verdict = PJV_CAUTION;
    return;
  }
  if (!status_.dnsOk || !status_.httpOk) {
    status_.verdict = PJV_NO_INTERNET;
    return;
  }
  if (status_.benchmarkAvgMs >= 0 && status_.benchmarkAvgMs > 5000) {
    status_.verdict = PJV_SLOW_CONNECTION;
    return;
  }
  // Heuristic: if DNS OK but HTTP not OK, treat as NO INTERNET; otherwise CAUTION.
  if (!status_.httpOk && status_.dnsOk) { status_.verdict = PJV_NO_INTERNET; return; }
  if (!status_.httpOk) { status_.verdict = PJV_CAUTION; return; }
  status_.verdict = PJV_READY_TO_USE;
}

void ProtectedJoin::update() {
  if (!active_) return;

  // Hard timeout while waiting for phone
  if (status_.phase == PJ_WAITING_FOR_PHONE) {
    g_dns.processNextRequest();
    g_server.handleClient();
    if (millis() - phaseStartMs_ > 5UL * 60UL * 1000UL) {
      status_.phase = PJ_RESULTS;
      status_.verdict = PJV_COULD_NOT_VERIFY;
      finalizeVerdict_();
    }
    return;
  }

  if (status_.phase == PJ_CONNECTING) {
    updateConnect_();
    if (status_.phase == PJ_RESULTS) {
      finalizeVerdict_();
    }
    return;
  }

  if (status_.phase == PJ_CHECKING) {
    status_.progressStep = 3;
    updateChecks_();
    if (checkPhase_ == 100) {
      status_.phase = PJ_RESULTS;
      finalizeVerdict_();
      WiFi.disconnect(true);
    } else if (checkPhase_ == 99) {
      status_.phase = PJ_RESULTS;
      finalizeVerdict_();
      WiFi.disconnect(true);
    }
    return;
  }
}

