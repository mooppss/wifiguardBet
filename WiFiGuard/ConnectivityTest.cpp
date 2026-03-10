#include "ConnectivityTest.h"
#include "Settings.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Arduino.h>
#include <string.h>

ConnectivityTest connectivityTest;

void ConnectivityTest::begin() {
  phase_ = 0;
  ssid_[0] = '\0';
  memset(&result_, 0, sizeof(result_));
  result_.grade = GRADE_UNTESTED;
  result_.portal = PORTAL_UNKNOWN;
  result_.portalUrl[0] = '\0';
}

void ConnectivityTest::start(const char* ssid) {
  strncpy(ssid_, ssid, 32);
  ssid_[32] = '\0';
  phase_ = 1;
  phaseStart_ = millis();
  result_.grade = GRADE_UNTESTED;
  result_.portal = PORTAL_UNKNOWN;
  result_.associationMs = result_.dnsMs = result_.httpMs = 0;
  result_.dnsOk = result_.httpOk = false;
  result_.httpCode = 0;
  result_.redirect = false;
  result_.benchmarkAvgMs = -1;
  result_.benchmarkJitterMs = -1;
  result_.benchmarkPings = 0;
  result_.portalUrl[0] = '\0';
  triedFallback_ = false;
  benchmarkIndex_ = 0;
  WiFi.disconnect();
  delay(50);
  WiFi.begin(ssid_);
}

void ConnectivityTest::setResultFromState() {
  uint32_t connMs = settings.get().connectTimeoutMs;
  uint32_t dnsMs = settings.get().dnsTimeoutMs;
  uint32_t httpMs = settings.get().httpTimeoutMs;
  if (phase_ == 1) {
    result_.associationMs = (int32_t)(millis() - phaseStart_);
    if (WiFi.status() != WL_CONNECTED) {
      result_.grade = GRADE_FAILED;
      result_.portal = PORTAL_FAILED;
    }
  } else if (phase_ == 2) {
    result_.dnsOk = true;  // set by phase 2 logic
    result_.dnsMs = (int32_t)(millis() - phaseStart_);
  } else if (phase_ == 3) {
    result_.httpMs = (int32_t)(millis() - phaseStart_);
    int32_t total = result_.associationMs + result_.dnsMs + result_.httpMs;
    if (result_.httpCode == 204 || (result_.httpCode >= 200 && result_.httpCode < 300)) {
      if (total < 2000) result_.grade = GRADE_FAST;
      else if (total < 5000) result_.grade = GRADE_NORMAL;
      else result_.grade = GRADE_SLOW;
      result_.portal = PORTAL_NORMAL;
    } else if (result_.redirect) {
      result_.grade = GRADE_PORTAL;
      result_.portal = PORTAL_REDIRECT_LOGIN;
    } else if (!result_.dnsOk) {
      result_.grade = GRADE_OFFLINE;
      result_.portal = PORTAL_NO_INTERNET;
    } else {
      result_.grade = GRADE_OFFLINE;
      result_.portal = PORTAL_NO_INTERNET;
    }
  }
}

bool ConnectivityTest::update() {
  if (phase_ == 0 || phase_ == 4) return (phase_ == 4);
  uint32_t now = millis();
  uint32_t connMs = settings.get().connectTimeoutMs;
  uint32_t dnsMs = settings.get().dnsTimeoutMs;
  uint32_t httpMs = settings.get().httpTimeoutMs;

  if (phase_ == 1) {
    if (WiFi.status() == WL_CONNECTED) {
      result_.associationMs = (int32_t)(now - phaseStart_);
      phase_ = 2;
      phaseStart_ = now;
    } else if ((now - phaseStart_) > connMs) {
      result_.associationMs = (int32_t)connMs;
      result_.grade = GRADE_FAILED;
      result_.portal = PORTAL_FAILED;
      phase_ = 4;
      WiFi.disconnect();
      return true;
    }
    return false;
  }

  if (phase_ == 2) {
    WiFiClient client;
    if (!client.connect("connectivitycheck.gstatic.com", 80, (int)dnsMs)) {
      result_.dnsOk = false;
      result_.dnsMs = (int32_t)(now - phaseStart_);
      result_.grade = GRADE_OFFLINE;
      result_.portal = PORTAL_NO_INTERNET;
      phase_ = 4;
      WiFi.disconnect();
      return true;
    }
    result_.dnsOk = true;
    result_.dnsMs = (int32_t)(now - phaseStart_);
    client.stop();
    phase_ = 3;
    phaseStart_ = now;
    return false;
  }

  if (phase_ == 3 || phase_ == 33) {
    HTTPClient http;
    http.setTimeout(httpMs / 1000);
    const char* url = (phase_ == 33) ? CONNECTIVITY_URL_ALT : CONNECTIVITY_URL;
    http.begin(url);
    int code = http.GET();
    result_.httpCode = code;
    result_.httpOk = (code > 0);
    result_.redirect = (code == 301 || code == 302);
    result_.httpMs = (int32_t)(now - phaseStart_);
    if (result_.redirect) {
      String loc = http.getLocation();
      if (loc.length() > 0) {
        strncpy(result_.portalUrl, loc.c_str(), sizeof(result_.portalUrl) - 1);
        result_.portalUrl[sizeof(result_.portalUrl) - 1] = '\0';
      }
    } else {
      result_.portalUrl[0] = '\0';
    }
    http.end();
    bool success = (code == 204 || (code >= 200 && code < 300));
    if (success || phase_ == 33) {
      setResultFromState();
      if (success && phase_ != 33) {
        phase_ = 5;
        benchmarkIndex_ = 0;
        return false;
      }
      phase_ = 4;
      WiFi.disconnect();
      return true;
    }
    if (!triedFallback_) {
      triedFallback_ = true;
      phase_ = 33;
      phaseStart_ = now;
      return false;
    }
    setResultFromState();
    phase_ = 4;
    WiFi.disconnect();
    return true;
  }
  if (phase_ == 5) {
    HTTPClient http;
    http.setTimeout(3);
    uint32_t start = millis();
    http.begin(CONNECTIVITY_URL);
    int code = http.GET();
    int elapsed = (int)(millis() - start);
    http.end();
    benchmarkTimes_[benchmarkIndex_] = (code > 0) ? elapsed : -1;
    benchmarkIndex_++;
    if (benchmarkIndex_ >= BENCHMARK_PINGS) {
      int sum = 0, count = 0, minT = 30000, maxT = 0;
      for (int i = 0; i < BENCHMARK_PINGS; i++) {
        if (benchmarkTimes_[i] > 0) {
          sum += benchmarkTimes_[i];
          count++;
          if (benchmarkTimes_[i] < minT) minT = benchmarkTimes_[i];
          if (benchmarkTimes_[i] > maxT) maxT = benchmarkTimes_[i];
        }
      }
      if (count > 0) {
        result_.benchmarkAvgMs = (int16_t)(sum / count);
        result_.benchmarkJitterMs = (int16_t)(maxT - minT);
        result_.benchmarkPings = (uint8_t)count;
      }
      phase_ = 4;
      WiFi.disconnect();
      return true;
    }
    return false;
  }

  return false;
}

void ConnectivityTest::getResult(ConnectivityResult& out) const {
  out = result_;
}
