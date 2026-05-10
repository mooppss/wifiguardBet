#include "UI.h"
#include "DisplayDriver.h"
#include "StateMachine.h"
#include "ScanHistory.h"
#include "Settings.h"
#include "RiskEngine.h"
#include "EnvironmentAnalysis.h"
#include "PowerManager.h"
#include "SessionStats.h"
#include "WiFiScanner.h"
#include "ConnectivityTest.h"
#include "ProtectedJoin.h"
#include <string.h>
#include <stdio.h>
#if defined(ESP32)
#include "esp_qrcode.h"
#endif

UI ui;

// ─── Layout constants (240 x 135 landscape) ────────────────────────────────
#define HDR_H          14
#define FTR_H          14
#define CONTENT_Y      (HDR_H + 1)
#define CONTENT_H      (SCREEN_H - HDR_H - 1 - FTR_H)
#define FTR_Y          (SCREEN_H - FTR_H)
#define ITEM_H_EXPERT  18
#define ITEM_H_SIMPLE  22
#define STATUS_H       12
#define ROW_H          12
#define SEL_BAR_W      3
#define MARGIN_L       4
#define MARGIN_R       2
#define CONTENT_W      (SCREEN_W - MARGIN_L - MARGIN_R)

// ─── Panel max widths (pixels) for text fit ──────────────────────────────────
#define PIX_CHAR_1     6
#define PIX_CHAR_2    12
#define HDR_TITLE_MAX (SCREEN_W - 50)
#define LIST_SSID_MAX (100)
#define LIST_BADGE_MAX (72)
#define LIST_SUMM_MAX (140)
#define DETAIL_RIGHT_W (SCREEN_W - 68 - MARGIN_L - MARGIN_R - 4)
#define EXPERT_DETAIL_RIGHT_W DETAIL_RIGHT_W
#define FTR_HINT_MAX  (SCREEN_W - MARGIN_L - MARGIN_R - 4)
#define TOAST_MAX     (SCREEN_W - MARGIN_L * 2 - 4)
#define SETTINGS_LABEL_W  (90)
#define SETTINGS_VAL_W    (SCREEN_W - 90 - MARGIN_L - MARGIN_R - 8)
#define DEBUG_LEFT_W     (110)
#define DEBUG_RIGHT_W    (SCREEN_W - 110 - MARGIN_L - MARGIN_R - 4)
#define EXPERT_ENV_COL_W (110)

// ─── Text-fit helpers (size 1 = 6px/char, size 2 = 12px/char) ─────────────────
static int measureTextWidth(const char* s, uint8_t textSize) {
  int len = (int)strlen(s);
  return len * (textSize == 2 ? PIX_CHAR_2 : PIX_CHAR_1);
}

static void truncateTextToWidth(char* dest, int destSize, const char* src, int maxPix, uint8_t textSize) {
  int step = (textSize == 2 ? PIX_CHAR_2 : PIX_CHAR_1);
  int maxChars = (maxPix - (step * 2)) / step;
  if (maxChars < 1) maxChars = 1;
  if (destSize <= 0) return;
  int len = 0;
  while (src[len] && len < maxChars && len < destSize - 1) len++;
  if (len == 0) { dest[0] = '\0'; return; }
  bool truncated = (src[len] != '\0');
  if (truncated && len > 2 && destSize > 3) { len -= 2; }
  strncpy(dest, src, len);
  dest[len] = '\0';
  if (truncated && len + 2 < destSize) { dest[len++] = '.'; dest[len++] = '.'; dest[len] = '\0'; }
}

static void drawClippedText(int x, int y, const char* s, int maxPix, uint8_t textSize, uint16_t fg, uint16_t bg) {
  static char buf[64];
  truncateTextToWidth(buf, sizeof(buf), s, maxPix, textSize);
  displayDriver.setTextColor(fg, bg);
  displayDriver.setTextSize(textSize);
  displayDriver.setCursor(x, y);
  displayDriver.print(buf);
}

static const char* fitBadgeText(UserVerdict v, int maxPix) {
  static const char* full[] = { "SAFE", "CAUTION", "AVOID", "LOGIN REQUIRED", "NO INTERNET", "TEST FIRST", "COULD NOT TEST" };
  static const char* short_[] = { "SAFE", "CAUT.", "AVOID", "LOGIN", "NO NET", "TEST", "FAIL" };
  int idx = (int)v;
  if (idx < 0 || idx > 6) return "?";
  int fullW = measureTextWidth(full[idx], 1);
  if (fullW <= maxPix) return full[idx];
  return short_[idx];
}

// ═══════════════════════════════════════════════════════════════════════════
//  VERDICT MAPPING LAYER — backbone of Simple Mode
// ═══════════════════════════════════════════════════════════════════════════

static UserVerdict getUserVerdict(const NetworkRecord& n) {
  if (n.grade == GRADE_PORTAL || n.portalResult == PORTAL_REDIRECT_LOGIN)
    return VERDICT_LOGIN_REQUIRED;
  if (n.grade == GRADE_OFFLINE || n.portalResult == PORTAL_NO_INTERNET)
    return VERDICT_NO_INTERNET;
  if (n.grade == GRADE_FAILED)
    return VERDICT_COULD_NOT_TEST;
  if (n.possibleEvilTwin || (n.riskScore >= 65 && n.duplicateClass >= DUP_SUSPICIOUS))
    return VERDICT_AVOID;
  if (n.riskScore >= 80)
    return VERDICT_AVOID;
  if (n.auth != AUTH_OPEN && n.auth != AUTH_UNKNOWN && n.riskScore < 50)
    return VERDICT_SAFE;
  if (n.auth == AUTH_OPEN && n.grade == GRADE_UNTESTED)
    return VERDICT_TEST_NEEDED;
  if (n.auth == AUTH_OPEN && (n.grade == GRADE_FAST || n.grade == GRADE_NORMAL) &&
      n.portalResult == PORTAL_NORMAL && n.riskScore < 35)
    return VERDICT_SAFE;
  return VERDICT_CAUTION;
}

static const char* getVerdictTitle(UserVerdict v) {
  switch (v) {
    case VERDICT_SAFE:            return "SAFE";
    case VERDICT_CAUTION:         return "CAUTION";
    case VERDICT_AVOID:           return "AVOID";
    case VERDICT_LOGIN_REQUIRED:  return "LOGIN REQUIRED";
    case VERDICT_NO_INTERNET:     return "NO INTERNET";
    case VERDICT_TEST_NEEDED:     return "TEST FIRST";
    case VERDICT_COULD_NOT_TEST:  return "COULD NOT TEST";
    default:                      return "?";
  }
}

static uint16_t getVerdictColor(UserVerdict v) {
  switch (v) {
    case VERDICT_SAFE:            return COL_SAFE;
    case VERDICT_CAUTION:         return COL_WARN;
    case VERDICT_AVOID:           return COL_DANGER;
    case VERDICT_LOGIN_REQUIRED:  return COL_WARN;
    case VERDICT_NO_INTERNET:     return COL_DANGER;
    case VERDICT_TEST_NEEDED:     return COL_INFO;
    case VERDICT_COULD_NOT_TEST:  return COL_DANGER;
    default:                      return COL_DIM;
  }
}

static const char* getVerdictReason(const NetworkRecord& n) {
  UserVerdict v = getUserVerdict(n);
  switch (v) {
    case VERDICT_SAFE:
      if (n.grade == GRADE_FAST || n.grade == GRADE_NORMAL)
        return "Protected, internet works";
      return "Protected with good signal";
    case VERDICT_CAUTION:
      if (n.auth == AUTH_OPEN) return "Open network, use carefully";
      if (n.rssi < -75)       return "Signal may be unreliable";
      return "Some warning signs detected";
    case VERDICT_AVOID:
      if (n.possibleEvilTwin)                  return "Possible copycat network";
      if (n.duplicateClass >= DUP_SUSPICIOUS)  return "Suspicious duplicate found";
      return "This network may be unsafe";
    case VERDICT_LOGIN_REQUIRED:  return "Sign-in page before internet";
    case VERDICT_NO_INTERNET:     return "Connected but no internet";
    case VERDICT_TEST_NEEDED:     return "Open network, not tested yet";
    case VERDICT_COULD_NOT_TEST:  return "Connection test failed";
    default: return "";
  }
}

static const char* getUserRecommendation(const NetworkRecord& n) {
  switch (getUserVerdict(n)) {
    case VERDICT_SAFE:            return "Okay to use";
    case VERDICT_CAUTION:         return "Use with caution";
    case VERDICT_AVOID:           return "Avoid this network";
    case VERDICT_LOGIN_REQUIRED:  return "Open sign-in page on phone";
    case VERDICT_NO_INTERNET:     return "No internet available";
    case VERDICT_TEST_NEEDED:     return "Test before using";
    case VERDICT_COULD_NOT_TEST:  return "Could not verify safety";
    default: return "";
  }
}

static void getSimpleReasons(const NetworkRecord& n, const char* out[], int& count, int maxR) {
  count = 0;
  if (n.auth == AUTH_OPEN)           { out[count++] = "No password required"; }
  else if (n.auth == AUTH_WEP)       { out[count++] = "Weak encryption (WEP)"; }
  else                               { out[count++] = "Password required"; }
  if (count < maxR && n.possibleEvilTwin)
    out[count++] = "Possible copycat network";
  else if (count < maxR && n.duplicateClass >= DUP_SAME_SSID && n.sameSSIDCount > 1)
    out[count++] = "Same name seen multiple times";
  if (count < maxR && n.rssi < -80)      out[count++] = "Very weak, may disconnect";
  else if (count < maxR && n.rssi < -70) out[count++] = "Signal is weak";
  else if (count < maxR && n.rssi > -50) out[count++] = "Strong signal";
  if (count < maxR && n.grade == GRADE_PORTAL)  out[count++] = "Sign-in page detected";
  if (count < maxR && n.grade == GRADE_SLOW)    out[count++] = "Connection is slow";
  if (count < maxR && (n.riskReasonsBitmask & (1 << RISK_HIDDEN)))
    out[count++] = "Network name is hidden";
}

static void getSimpleSummary(const NetworkRecord& n, char* buf, int bufLen) {
  const char* auth = (n.auth == AUTH_OPEN) ? "No password" : "Password";
  const char* sig;
  if      (n.rssi > -50) sig = "Strong";
  else if (n.rssi > -70) sig = "Good";
  else if (n.rssi > -80) sig = "Weak";
  else                    sig = "Very weak";
  snprintf(buf, bufLen, "%s | %s signal", auth, sig);
}

static int verdictPriority(UserVerdict v) {
  switch (v) {
    case VERDICT_SAFE:            return 0;
    case VERDICT_TEST_NEEDED:     return 1;
    case VERDICT_LOGIN_REQUIRED:  return 2;
    case VERDICT_CAUTION:         return 3;
    case VERDICT_COULD_NOT_TEST:  return 4;
    case VERDICT_NO_INTERNET:     return 5;
    case VERDICT_AVOID:           return 6;
    default: return 7;
  }
}

// ─── Shared helpers ─────────────────────────────────────────────────────────

static void truncSSID(const char* ssid, char* out, int maxChars) {
  int len = strlen(ssid);
  if (len <= maxChars) {
    strncpy(out, ssid, maxChars + 1);
  } else {
    strncpy(out, ssid, maxChars - 2);
    out[maxChars - 2] = '.';
    out[maxChars - 1] = '.';
    out[maxChars] = '\0';
  }
}

// ─── Expert mode string tables ──────────────────────────────────────────────
static const char* authStrings[] = { "Open", "WEP", "WPA", "WPA2", "WPA3", "WPA/2", "WPA2/3", "?" };
static const char* gradeStrings[] = { "Good", "OK", "Slow", "Portal!", "No Internet", "Secured", "FAILED", "Not tested" };
static const char* portalStrings[] = { "None", "Login page", "Hijacked!", "No internet", "Error", "Auth needed", "?" };

// ─── Color helpers ──────────────────────────────────────────────────────────
static uint16_t riskColor(uint8_t score) {
  if (score >= 65) return COL_DANGER;
  if (score >= 35) return COL_WARN;
  return COL_SAFE;
}

static uint16_t authColor(AuthType a) {
  switch (a) {
    case AUTH_OPEN:      return COL_DANGER;
    case AUTH_WEP:
    case AUTH_WPA:
    case AUTH_WPA_WPA2:  return COL_WARN;
    case AUTH_WPA2:
    case AUTH_WPA3:
    case AUTH_WPA2_WPA3: return COL_SAFE;
    default:             return COL_DIM;
  }
}

static uint16_t gradeColor(ConnectivityGrade g) {
  switch (g) {
    case GRADE_FAST:      return COL_SAFE;
    case GRADE_NORMAL:    return COL_INFO;
    case GRADE_SLOW:      return COL_WARN;
    case GRADE_PORTAL:    return COL_WARN;
    case GRADE_OFFLINE:
    case GRADE_FAILED:    return COL_DANGER;
    default:              return COL_DIM;
  }
}

// ─── Member string helpers ──────────────────────────────────────────────────
const char* UI::authStr(AuthType a) const {
  if ((unsigned)a >= 8) return "?";
  return authStrings[a];
}
const char* UI::gradeStr(ConnectivityGrade g) const {
  if ((unsigned)g >= 8) return "-";
  return gradeStrings[g];
}
const char* UI::portalStr(PortalResult p) const {
  if ((unsigned)p >= 7) return "?";
  return portalStrings[p];
}

// ═══════════════════════════════════════════════════════════════════════════
//  INIT / TOAST
// ═══════════════════════════════════════════════════════════════════════════

void UI::begin() {
  dirty_ = true;
  view_ = VIEW_LIST;
  listIndex_ = 0;
  listScroll_ = 0;
  filteredCount_ = 0;
  lastRedraw_ = 0;
  settingsOptionIndex_ = 0;
  highRiskAlert_ = false;
  pjShowQR_ = false;
  toast_[0] = '\0';
  toastUntil_ = 0;
  envPage_ = 0;
  lastDrawnState_ = (DeviceState)0xFF;
  lastDrawnView_ = (UIView)0xFF;
  memset(sortedIndices_, 0, sizeof(sortedIndices_));
}

void UI::setToast(const char* msg) {
  strncpy(toast_, msg, sizeof(toast_) - 1);
  toast_[sizeof(toast_) - 1] = '\0';
  toastUntil_ = millis() + 3000;
  dirty_ = true;
}

void UI::clearToast() {
  toast_[0] = '\0';
  toastUntil_ = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
//  HEADER / FOOTER
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawHeader(const char* title) {
  uint16_t chromeBg = settings.get().highContrast ? (uint16_t)0xFFFF : COL_CHROME;
  uint16_t chromeFg = settings.get().highContrast ? (uint16_t)0x0000 : COL_FG;
  displayDriver.fillRect(0, 0, SCREEN_W, HDR_H, chromeBg);
  displayDriver.setTextSize(1);
  if (settings.get().demoMode) {
    static char demoTitle[32];
    snprintf(demoTitle, sizeof(demoTitle), "%s (Demo)", title);
    drawClippedText(MARGIN_L, 3, demoTitle, HDR_TITLE_MAX, 1, chromeFg, chromeBg);
  } else {
    drawClippedText(MARGIN_L, 3, title, HDR_TITLE_MAX, 1, chromeFg, chromeBg);
  }
  char bat[8];
  snprintf(bat, sizeof(bat), "%u%%", (unsigned)powerManager.getBatteryPct());
  int batW = strlen(bat) * PIX_CHAR_1;
  displayDriver.setTextColor(chromeFg, chromeBg);
  displayDriver.setCursor(SCREEN_W - batW - MARGIN_R, 3);
  displayDriver.print(bat);
  displayDriver.drawLine(0, HDR_H, SCREEN_W - 1, HDR_H, settings.get().highContrast ? (uint16_t)0x0000 : COL_DIM);
}

void UI::drawFooter(const char* hints) {
  uint16_t chromeBg = settings.get().highContrast ? (uint16_t)0xFFFF : COL_CHROME;
  uint16_t chromeFg = settings.get().highContrast ? (uint16_t)0x0000 : COL_DIM;
  displayDriver.fillRect(0, FTR_Y, SCREEN_W, FTR_H, chromeBg);
  displayDriver.setTextSize(1);
  drawClippedText(MARGIN_L, FTR_Y + 3, hints, FTR_HINT_MAX, 1, chromeFg, chromeBg);
}

// ─── Signal strength bars (4 bars, filled by RSSI) ─────────────────────────────
void UI::drawSignalBars(int x, int y, int8_t rssi) {
  int bars = (rssi >= -50) ? 4 : (rssi >= -60) ? 3 : (rssi >= -70) ? 2 : (rssi >= -80) ? 1 : 0;
  uint16_t col = (rssi >= -60) ? COL_SAFE : (rssi >= -75) ? COL_WARN : COL_DANGER;
  const int W = 2, G = 1, H[] = { 2, 4, 6, 8 };
  for (int i = 0; i < 4; i++) {
    int xi = x + i * (W + G);
    displayDriver.fillRect(xi, y + (8 - H[i]), W, H[i], i < bars ? col : COL_CHROME);
  }
}

// ─── Portal assist: show URL for phone, safety tip ───────────────────────────
void UI::drawPortalAssist(int x, int y, const char* url) {
  displayDriver.setTextSize(1);
  drawClippedText(x, y, "Open sign-in page on phone", DETAIL_RIGHT_W, 1, COL_INFO, COL_BG);
  y += 11;
  if (url && url[0]) {
    static char urlBuf[48];
    truncateTextToWidth(urlBuf, sizeof(urlBuf), url, DETAIL_RIGHT_W, 1);
    drawClippedText(x, y, urlBuf, DETAIL_RIGHT_W, 1, COL_DIM, COL_BG);
    y += 11;
  }
  drawClippedText(x, y, "Don't enter passwords on sign-in", DETAIL_RIGHT_W, 1, COL_WARN, COL_BG);
}

// ─── Suspicious portal warning (show first when portalSafety == PORTAL_SUSPICIOUS) ─
void UI::drawPortalSuspiciousWarning() {
  const NetworkRecord* n = getNetworkAtDisplayIndex(listIndex_);
  drawHeader("SUSPICIOUS LOGIN PAGE");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  if (!n || !n->portalUrl[0]) {
    drawFooter("R=Back");
    return;
  }
  int y = CONTENT_Y + 8;
  displayDriver.setTextSize(1);
  displayDriver.setTextColor(COL_WARN, COL_BG);
  displayDriver.setCursor(MARGIN_L, y);
  displayDriver.print("Possible phishing portal");
  y += 12;
  displayDriver.setCursor(MARGIN_L, y);
  displayDriver.print("detected.");
  y += 14;
  displayDriver.setTextColor(COL_DIM, COL_BG);
  displayDriver.setCursor(MARGIN_L, y);
  displayDriver.print("Portal domain:");
  y += 11;
  if (n->portalDomain[0]) {
    static char domBuf[52];
    truncateTextToWidth(domBuf, sizeof(domBuf), n->portalDomain, SCREEN_W - MARGIN_L - MARGIN_R, 1);
    drawClippedText(MARGIN_L, y, domBuf, SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_FG, COL_BG);
    y += 11;
  }
  y += 6;
  displayDriver.setTextColor(COL_WARN, COL_BG);
  drawClippedText(MARGIN_L, y, "Recommendation: Avoid entering", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_WARN, COL_BG);
  y += 11;
  drawClippedText(MARGIN_L, y, "personal information.", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_WARN, COL_BG);
  drawFooter("Hold L=Show link anyway   R=Back");
}

// ─── QR screen (portal URL as QR; user opened via Hold L on portal detail) ─────
// Safety: QR/link display is user-initiated only; device does not auto-submit or store credentials.
void UI::drawQRScreen() {
  const NetworkRecord* n = getNetworkAtDisplayIndex(listIndex_);
  drawHeader("Portal link");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  if (!n || !n->portalUrl[0]) {
    drawFooter("R=Back");
    return;
  }
  const char* url = n->portalUrl;
  if (strlen(url) > 90) {
    static char domainUrl[64];
    snprintf(domainUrl, sizeof(domainUrl), "https://%s", n->portalDomain[0] ? n->portalDomain : "unknown");
    url = domainUrl;
  }
  int modulePx = 2;
  int qrSize = 37 * modulePx;  // version 5 = 37 modules
  int cx = (SCREEN_W - qrSize) / 2;
  int cy = CONTENT_Y + (CONTENT_H - qrSize - 12) / 2;
  drawQRCode(cx, cy, modulePx, url);
  displayDriver.setTextSize(1);
  displayDriver.setTextColor(COL_DIM, COL_BG);
  displayDriver.setCursor(MARGIN_L, FTR_Y - 20);
  displayDriver.print("Scan to open on phone");
  drawFooter("R=Back");
}

// ─── QR code render — uses ESP32 built-in esp_qrcode (no extra library needed) ─────────────────────
#if defined(ESP32)
// File-scope context lets the static callback know where to draw.
static struct { int x; int y; int px; } s_qrCtx;

static void s_qrDrawCb(esp_qrcode_handle_t handle) {
  uint8_t sz = esp_qrcode_get_size(handle);
  // White quiet zone + background: QR spec requires 4-module border; 2px padding is enough for phones.
  int pad = s_qrCtx.px * 2;
  displayDriver.fillRect(s_qrCtx.x - pad, s_qrCtx.y - pad,
                         sz * s_qrCtx.px + pad * 2, sz * s_qrCtx.px + pad * 2,
                         0xFFFF);  // always white — QR must be black-on-white
  for (uint8_t row = 0; row < sz; row++) {
    for (uint8_t col = 0; col < sz; col++) {
      if (esp_qrcode_get_module(handle, col, row)) {
        displayDriver.fillRect(s_qrCtx.x + col * s_qrCtx.px,
                               s_qrCtx.y + row * s_qrCtx.px,
                               s_qrCtx.px, s_qrCtx.px,
                               0x0000);  // always black
      }
    }
  }
}
#endif

void UI::drawQRCode(int x, int y, int moduleSizePx, const char* url) {
#if defined(ESP32)
  s_qrCtx.x  = x;
  s_qrCtx.y  = y;
  s_qrCtx.px = moduleSizePx;
  esp_qrcode_config_t cfg;
  cfg.display_qrcode    = s_qrDrawCb;
  cfg.max_qrcode_version = 10;
  cfg.ecc_level          = ESP_QRCODE_ECC_LOW;
  esp_qrcode_generate(&cfg, url);
#else
  (void)url;
  displayDriver.setTextSize(1);
  displayDriver.setTextColor(COL_DIM, COL_BG);
  displayDriver.setCursor(x, y + 20);
  displayDriver.print("QR not supported");
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
//  SORT & FILTER
// ═══════════════════════════════════════════════════════════════════════════

static void sortIndicesExpert(uint16_t* indices, int n, const ScanRecord* scan) {
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      uint16_t ii = indices[i], jj = indices[j];
      const NetworkRecord* ni = &scan->networks[ii];
      const NetworkRecord* nj = &scan->networks[jj];
      int cmp = 0;
      SortMode mode = settings.get().sortMode;
      switch (mode) {
        case SORT_RSSI: cmp = nj->rssi - ni->rssi; break;
        case SORT_RISK: cmp = (int)ni->riskScore - (int)nj->riskScore; break;
        case SORT_OPEN_FIRST: cmp = (ni->auth == AUTH_OPEN ? 0 : 1) - (nj->auth == AUTH_OPEN ? 0 : 1); break;
        case SORT_ENCRYPTED_FIRST: cmp = (ni->auth != AUTH_OPEN ? 0 : 1) - (nj->auth != AUTH_OPEN ? 0 : 1); break;
        case SORT_CHANNEL: cmp = (int)ni->channel - (int)nj->channel; break;
        case SORT_PORTAL_FIRST: cmp = (ni->grade == GRADE_PORTAL ? 0 : 1) - (nj->grade == GRADE_PORTAL ? 0 : 1); break;
        case SORT_DUPLICATE_FIRST: cmp = (ni->duplicateSSID ? 0 : 1) - (nj->duplicateSSID ? 0 : 1); break;
        case SORT_NEWEST: cmp = (nj->newThisScan ? 0 : 1) - (ni->newThisScan ? 0 : 1); break;
        default: cmp = nj->rssi - ni->rssi;
      }
      if (cmp > 0) { indices[i] = jj; indices[j] = ii; }
    }
  }
}

static void sortIndicesSimple(uint16_t* indices, int n, const ScanRecord* scan) {
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      const NetworkRecord* ni = &scan->networks[indices[i]];
      const NetworkRecord* nj = &scan->networks[indices[j]];
      int pi = verdictPriority(getUserVerdict(*ni));
      int pj = verdictPriority(getUserVerdict(*nj));
      bool swap = (pi > pj) || (pi == pj && nj->rssi > ni->rssi);
      if (swap) { uint16_t t = indices[i]; indices[i] = indices[j]; indices[j] = t; }
    }
  }
}

void UI::applySortFilter() {
  const ScanRecord* scan = scanHistory.getCurrentScan();
  if (!scan) { filteredCount_ = 0; return; }
  uint8_t filter = settings.get().filterFlags;
  int k = 0;
  for (uint16_t i = 0; i < scan->networkCount && k < MAX_NETWORKS; i++) {
    const NetworkRecord* n = &scan->networks[i];
    if ((filter & FILTER_HIDE_HIDDEN) && n->hidden) continue;
    if ((filter & FILTER_OPEN_ONLY) && n->auth != AUTH_OPEN) continue;
    if ((filter & FILTER_RISKY_ONLY) && n->riskScore < 50) continue;
    if ((filter & FILTER_HIDE_LOW_SIGNAL) && n->rssi < -85) continue;
    sortedIndices_[k++] = i;
  }
  filteredCount_ = k;
  if (settings.get().expertMode)
    sortIndicesExpert(sortedIndices_, k, scan);
  else
    sortIndicesSimple(sortedIndices_, k, scan);
}

const NetworkRecord* UI::getNetworkAtDisplayIndex(int i) const {
  const ScanRecord* scan = scanHistory.getCurrentScan();
  if (!scan || i < 0 || i >= filteredCount_) return nullptr;
  return &scan->networks[sortedIndices_[i]];
}

uint16_t UI::getSortedIndices(uint16_t* out, int maxLen) const {
  int n = filteredCount_;
  if (n > maxLen) n = maxLen;
  for (int i = 0; i < n; i++) out[i] = sortedIndices_[i];
  return (uint16_t)n;
}

static const char* sortModeStr(SortMode m) {
  switch (m) {
    case SORT_RSSI:            return "Signal";
    case SORT_RISK:            return "Risk";
    case SORT_OPEN_FIRST:      return "Open 1st";
    case SORT_ENCRYPTED_FIRST: return "Enc 1st";
    case SORT_CHANNEL:         return "Channel";
    case SORT_NEWEST:          return "Newest";
    case SORT_PORTAL_FIRST:    return "Portal 1st";
    case SORT_DUPLICATE_FIRST: return "Dup 1st";
    default: return "?";
  }
}

static const char* filterStr(uint8_t f) {
  if (f == 0) return "None";
  if (f == FILTER_HIDE_HIDDEN) return "Hide hidden";
  if (f == FILTER_OPEN_ONLY) return "Open only";
  if (f == FILTER_RISKY_ONLY) return "Risky only";
  if (f == FILTER_HIDE_LOW_SIGNAL) return "Hide weak";
  return "?";
}

// ═══════════════════════════════════════════════════════════════════════════
//  DRAW DISPATCHER
// ═══════════════════════════════════════════════════════════════════════════

void UI::draw() {
  uint32_t now = millis();
  if (toast_[0] && now > toastUntil_) { toast_[0] = '\0'; dirty_ = true; }
  if (!dirty_) return;
  if (now - lastRedraw_ < REDRAW_THROTTLE_MS && lastRedraw_ != 0) return;
  lastRedraw_ = now;
  dirty_ = false;

  DeviceState st = stateMachine.getState();
  if (st == STATE_SETTINGS) view_ = VIEW_SETTINGS;
  UIView curView = (st == STATE_BROWSING) ? view_ : (st == STATE_SETTINGS ? VIEW_SETTINGS : VIEW_LIST);
  bool screenChanged = (st != lastDrawnState_) || (curView != lastDrawnView_);
  if (screenChanged) {
    displayDriver.fillScreen(COL_BG);
    lastDrawnState_ = st;
    lastDrawnView_ = curView;
  }

  bool simple = !settings.get().expertMode;

  if (st == STATE_IDLE)            drawIdle();
  else if (st == STATE_SCANNING)   drawScanning();
  else if (st == STATE_PROCESSING) { setView(VIEW_LIST); simple ? drawListSimple() : drawListExpert(); }
  else if (st == STATE_BROWSING) {
    if (view_ == VIEW_LIST)         { simple ? drawListSimple() : drawListExpert(); }
    else if (view_ == VIEW_DETAIL)  { simple ? drawDetailSimple() : drawDetailExpert(); }
    else if (view_ == VIEW_PORTAL_SUSPICIOUS) drawPortalSuspiciousWarning();
    else if (view_ == VIEW_QR)      drawQRScreen();
    else if (view_ == VIEW_ENV_SUMMARY) { simple ? drawEnvSummarySimple() : drawEnvSummaryExpert(); }
    else if (view_ == VIEW_SETTINGS) drawSettings();
    else if (view_ == VIEW_HELP)    drawHelp();
  }
  else if (st == STATE_TESTING)    { simple ? drawTestingSimple() : drawTestingExpert(); }
  else if (st == STATE_PROTECTED_JOIN) drawProtectedJoin();
  else if (st == STATE_EXPORT)     drawExport();
  else if (st == STATE_SETTINGS)   { if (view_ == VIEW_HELP) drawHelp(); else drawSettings(); }
  else if (st == STATE_STABILITY_MONITOR) drawStabilityMonitor();
  else if (st == STATE_DEBUG)      drawDebug();
  else if (st == STATE_SLEEP)      drawSleep();
}

// ═══════════════════════════════════════════════════════════════════════════
//  PROTECTED JOIN (phone-assisted) — defensive join + verification
// ═══════════════════════════════════════════════════════════════════════════

static const char* pjVerdictTitle(ProtectedJoinVerdict v) {
  switch (v) {
    case PJV_READY_TO_USE: return "READY TO USE";
    case PJV_LOGIN_REQUIRED: return "LOGIN REQUIRED";
    case PJV_NO_INTERNET: return "NO INTERNET";
    case PJV_SLOW_CONNECTION: return "SLOW CONNECTION";
    case PJV_CAUTION: return "CAUTION";
    case PJV_SUSPICIOUS_BEHAVIOR: return "SUSPICIOUS";
    default: return "COULD NOT VERIFY";
  }
}

void UI::drawProtectedJoin() {
  ProtectedJoinStatus st = protectedJoin.getStatus();
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);

  if (st.phase == PJ_WAITING_FOR_PHONE) {
    if (pjShowQR_) {
      // ── QR view: scan with phone camera to auto-join the setup AP ───────────
      // Payload follows the WIFI: URI scheme understood natively by iOS 11+ and
      // Android 10+ camera apps — no third-party scanner needed.
      drawHeader("Scan QR to connect");
      char qrBuf[96];
      snprintf(qrBuf, sizeof(qrBuf), "WIFI:T:WPA;S:%s;P:%s;;",
               protectedJoin.getSetupApSsid(), protectedJoin.getSetupApPass());
      int modulePx = 2;
      int qrPx = 37 * modulePx;  // version 5 = 37 modules
      int qx = (SCREEN_W - qrPx) / 2;
      int qy = CONTENT_Y + 2;
      drawQRCode(qx, qy, modulePx, qrBuf);
      // Show pairing code below so the user doesn't lose it while on QR view
      char codeBuf[24];
      snprintf(codeBuf, sizeof(codeBuf), "Code: %s", protectedJoin.getPairingCode());
      displayDriver.setTextSize(1);
      displayDriver.setTextColor(COL_WARN, COL_BG);
      int textY = qy + qrPx + 4;
      if (textY < FTR_Y - 10) {
        displayDriver.setCursor((SCREEN_W - (int)strlen(codeBuf) * PIX_CHAR_1) / 2, textY);
        displayDriver.print(codeBuf);
      }
      drawFooter("L=Text view  R=Cancel");
    } else {
      // ── Text view ────────────────────────────────────────────────────────────
      drawHeader("Secure Join");
      int y = CONTENT_Y + 6;
      drawClippedText(MARGIN_L, y, "Enter password on your phone", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_FG, COL_BG);
      y += 12;
      drawClippedText(MARGIN_L, y, "Only for networks you are allowed", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_DIM, COL_BG);
      y += 14;
      char buf[64];
      snprintf(buf, sizeof(buf), "1) Join: %s", protectedJoin.getSetupApSsid());
      drawClippedText(MARGIN_L, y, buf, SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_INFO, COL_BG);
      y += 11;
      snprintf(buf, sizeof(buf), "2) Pass: %s", protectedJoin.getSetupApPass());
      drawClippedText(MARGIN_L, y, buf, SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_INFO, COL_BG);
      y += 11;
      drawClippedText(MARGIN_L, y, "3) Open: http://192.168.4.1", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_DIM, COL_BG);
      y += 11;
      snprintf(buf, sizeof(buf), "Pairing code: %s", protectedJoin.getPairingCode());
      drawClippedText(MARGIN_L, y, buf, SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_WARN, COL_BG);
      drawFooter("L=QR code  R=Cancel");
    }
    return;
  }

  if (st.phase == PJ_CONNECTING) {
    drawHeader("Connecting");
    int y = CONTENT_Y + 14;
    drawClippedText(MARGIN_L, y, st.targetSsid[0] ? st.targetSsid : "(unknown)", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_FG, COL_BG);
    y += 18;
    drawClippedText(MARGIN_L, y, "Joining Wi-Fi...", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_DIM, COL_BG);
    drawFooter("R=Cancel");
    return;
  }

  if (st.phase == PJ_CHECKING) {
    drawHeader("Verifying");
    int y = CONTENT_Y + 14;
    drawClippedText(MARGIN_L, y, st.targetSsid[0] ? st.targetSsid : "(unknown)", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_FG, COL_BG);
    y += 18;
    drawClippedText(MARGIN_L, y, "Checking internet & portal...", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_DIM, COL_BG);
    drawFooter("R=Cancel");
    return;
  }

  if (st.phase == PJ_RESULTS) {
    const char* title = pjVerdictTitle(st.verdict);
    drawHeader(title);
    int y = CONTENT_Y + 8;
    char line[64];
    snprintf(line, sizeof(line), "Network: %s", st.targetSsid[0] ? st.targetSsid : "(unknown)");
    drawClippedText(MARGIN_L, y, line, SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_DIM, COL_BG);
    y += 12;

    if (st.verdict == PJV_LOGIN_REQUIRED || st.verdict == PJV_CAUTION || st.verdict == PJV_SUSPICIOUS_BEHAVIOR) {
      if (st.portalDomain[0]) {
        snprintf(line, sizeof(line), "Portal: %s", st.portalDomain);
        drawClippedText(MARGIN_L, y, line, SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_INFO, COL_BG);
        y += 11;
      }
      const char* ps = (st.portalSafety == PORTAL_SAFE) ? "SAFE" : (st.portalSafety == PORTAL_CAUTION) ? "CAUTION" : "SUSPICIOUS";
      snprintf(line, sizeof(line), "Portal safety: %s (%u)", ps, (unsigned)st.portalSafetyScore);
      drawClippedText(MARGIN_L, y, line, SCREEN_W - MARGIN_L - MARGIN_R, 1,
        st.portalSafety == PORTAL_SAFE ? COL_SAFE : (st.portalSafety == PORTAL_CAUTION ? COL_WARN : COL_DANGER), COL_BG);
      y += 11;
      drawClippedText(MARGIN_L, y, "Tip: Avoid entering personal info.", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_WARN, COL_BG);
      y += 11;
    } else if (st.verdict == PJV_READY_TO_USE) {
      drawClippedText(MARGIN_L, y, "Internet works normally.", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_SAFE, COL_BG);
      y += 11;
      drawClippedText(MARGIN_L, y, "Tip: Use VPN on public Wi-Fi.", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_WARN, COL_BG);
      y += 11;
    } else if (st.verdict == PJV_NO_INTERNET) {
      drawClippedText(MARGIN_L, y, "Connected, but internet not available.", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_WARN, COL_BG);
      y += 11;
      drawClippedText(MARGIN_L, y, "Try again or ask staff.", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_DIM, COL_BG);
      y += 11;
    } else if (st.verdict == PJV_SLOW_CONNECTION) {
      drawClippedText(MARGIN_L, y, "Internet works but is slow/unstable.", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_WARN, COL_BG);
      y += 11;
    } else {
      drawClippedText(MARGIN_L, y, "WiFiGuard couldn't verify this network.", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_WARN, COL_BG);
      y += 11;
      drawClippedText(MARGIN_L, y, "Check password and retry.", SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_DIM, COL_BG);
      y += 11;
    }

    if (st.benchmarkAvgMs >= 0) {
      snprintf(line, sizeof(line), "Speed: %dms  Jitter: %dms", st.benchmarkAvgMs, st.benchmarkJitterMs);
      drawClippedText(MARGIN_L, y, line, SCREEN_W - MARGIN_L - MARGIN_R, 1, COL_DIM, COL_BG);
      y += 11;
    }

    drawFooter("L=Retry  Hold L=Trust  R=Back");
    return;
  }

  drawHeader("Secure Join");
  drawFooter("R=Back");
}

// ═══════════════════════════════════════════════════════════════════════════
//  IDLE
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawIdle() {
  drawHeader("WiFiGuard");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);

  int y = CONTENT_Y + 8;
  displayDriver.setTextSize(2);
  displayDriver.setTextColor(COL_FG, COL_BG);
  displayDriver.setCursor(54, y);
  displayDriver.print("WiFiGuard");
  y += 22;

  displayDriver.setTextSize(1);
  displayDriver.setTextColor(COL_DIM, COL_BG);
  displayDriver.setCursor(66, y);
  displayDriver.print("WiFi Safety Checker");
  y += 18;

  const ScanRecord* prev = scanHistory.getLatestPtr();
  if (prev && prev->networkCount > 0) {
    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setCursor(MARGIN_L, y);
    displayDriver.print("---- Last check ----");
    y += ROW_H;
    char buf[48];
    uint16_t openC = 0, riskyC = 0;
    for (uint16_t i = 0; i < prev->networkCount; i++) {
      if (prev->networks[i].auth == AUTH_OPEN) openC++;
      if (prev->networks[i].riskScore >= 65) riskyC++;
    }
    snprintf(buf, sizeof(buf), "%u networks  |  %u open  |  %u risky",
             (unsigned)prev->networkCount, openC, riskyC);
    drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_FG, COL_BG);
  } else {
    drawClippedText(MARGIN_L, y, "Hold LEFT to check nearby WiFi", CONTENT_W, 1, COL_DIM, COL_BG);
  }

  drawFooter("Hold L=Check  Hold R=Menu");
}

// ═══════════════════════════════════════════════════════════════════════════
//  SCANNING
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawScanning() {
  drawHeader("Checking WiFi");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);

  int y = CONTENT_Y + 14;
  displayDriver.setTextSize(2);
  displayDriver.setTextColor(COL_SAFE, COL_BG);
  displayDriver.setCursor(24, y);
  displayDriver.print("Checking WiFi");
  y += 26;

  displayDriver.setTextSize(1);
  displayDriver.setTextColor(COL_DIM, COL_BG);
  displayDriver.setCursor(48, y);
  displayDriver.print("Finding nearby networks");
  y += 20;

  uint8_t dots = (millis() / 500) % 4;
  displayDriver.fillRect(48, y, 80, 12, COL_BG);
  displayDriver.setCursor(48, y);
  displayDriver.setTextColor(COL_FG, COL_BG);
  for (uint8_t d = 0; d <= dots; d++) displayDriver.print(". ");

  drawFooter("Hold R=Stop");
}

// ═══════════════════════════════════════════════════════════════════════════
//  LIST — SIMPLE MODE (recommendation-oriented)
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawListSimple() {
  applySortFilter();
  drawHeader("Nearby WiFi");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);

  const ScanRecord* scan = scanHistory.getCurrentScan();
  if (!scan || filteredCount_ == 0) {
    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setTextSize(1);
    displayDriver.setCursor(MARGIN_L, CONTENT_Y + 20);
    displayDriver.print("No networks found.");
    displayDriver.setCursor(MARGIN_L, CONTENT_Y + 34);
    displayDriver.print("Hold LEFT to check again.");
    drawFooter("Hold L=Check  R=Back");
    return;
  }

  int itemH = ITEM_H_SIMPLE;
  int listH = CONTENT_H - STATUS_H;
  int visible = listH / itemH;
  if (visible < 1) visible = 1;

  if (listIndex_ >= filteredCount_) listIndex_ = filteredCount_ - 1;
  if (listScroll_ > listIndex_) listScroll_ = listIndex_;
  if (listScroll_ < listIndex_ - visible + 1) listScroll_ = listIndex_ - visible + 1;
  if (listScroll_ < 0) listScroll_ = 0;

  displayDriver.setTextSize(1);
  for (int i = 0; i < visible; i++) {
    int idx = listScroll_ + i;
    if (idx >= filteredCount_) break;
    const NetworkRecord* n = getNetworkAtDisplayIndex(idx);
    if (!n) continue;
    int y = CONTENT_Y + i * itemH;
    bool sel = (idx == listIndex_);

    if (sel) displayDriver.fillRect(0, y, SEL_BAR_W, itemH - 1, COL_INFO);

    int textX = MARGIN_L + SEL_BAR_W + 2;
    drawSignalBars(textX, y + 3, n->rssi);
    textX += 15;
    UserVerdict v = getUserVerdict(*n);
    const char* badgeStr = fitBadgeText(v, LIST_BADGE_MAX);
    int badgeW = measureTextWidth(badgeStr, 1) + 4;
    int vx = SCREEN_W - badgeW - MARGIN_R - 2;
    int ssidMax = (vx - textX - 4);
    if (ssidMax < 20) ssidMax = 20;

    displayDriver.fillRect(textX, y + 1, ssidMax + 4, 10, COL_BG);
    drawClippedText(textX, y + 2, n->hidden ? "(hidden)" : n->ssid, ssidMax, 1, COL_FG, COL_BG);

    uint16_t vc = getVerdictColor(v);
    displayDriver.fillRect(vx - 1, y + 1, badgeW + 2, 10, vc);
    displayDriver.setTextColor(COL_BG, vc);
    displayDriver.setCursor(vx + 1, y + 2);
    displayDriver.print(badgeStr);

    static char summBuf[32];
    getSimpleSummary(*n, summBuf, sizeof(summBuf));
    displayDriver.fillRect(textX, y + 12, ssidMax + 4, 10, COL_BG);
    drawClippedText(textX, y + 13, summBuf, ssidMax, 1, COL_DIM, COL_BG);

    displayDriver.drawLine(textX, y + itemH - 1, SCREEN_W - 4, y + itemH - 1, COL_CHROME);
  }

  // Scroll indicator
  if (filteredCount_ > visible) {
    int trackH = listH;
    int thumbH = trackH * visible / filteredCount_;
    if (thumbH < 6) thumbH = 6;
    int scrollRange = filteredCount_ - visible;
    int thumbY = CONTENT_Y + (trackH - thumbH) * listScroll_ / (scrollRange > 0 ? scrollRange : 1);
    displayDriver.fillRect(SCREEN_W - 2, CONTENT_Y, 2, trackH, COL_CHROME);
    displayDriver.fillRect(SCREEN_W - 2, thumbY, 2, thumbH, COL_DIM);
  }

  if (toast_[0]) {
    int toastY = FTR_Y - 12;
    displayDriver.fillRect(0, toastY, SCREEN_W, 11, COL_WARN);
    drawClippedText(MARGIN_L, toastY + 1, toast_, TOAST_MAX, 1, COL_BG, COL_WARN);
  }

  int statusY = CONTENT_Y + CONTENT_H - STATUS_H;
  displayDriver.fillRect(0, statusY, SCREEN_W, STATUS_H, COL_BG);
  displayDriver.setTextColor(COL_DIM, COL_BG);
  char statusBuf[32];
  snprintf(statusBuf, sizeof(statusBuf), "%d nearby", filteredCount_);
  drawClippedText(MARGIN_L, statusY + 2, statusBuf, SCREEN_W - MARGIN_L - 4, 1, COL_DIM, COL_BG);

  drawFooter("L=Next  Hold L=Select  R=Back  Hold R=Menu");
}

// ═══════════════════════════════════════════════════════════════════════════
//  LIST — EXPERT MODE (technical, current layout)
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawListExpert() {
  applySortFilter();
  drawHeader("Networks");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);

  const ScanRecord* scan = scanHistory.getCurrentScan();
  if (!scan || filteredCount_ == 0) {
    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setTextSize(1);
    displayDriver.setCursor(MARGIN_L, CONTENT_Y + 20);
    displayDriver.print("No networks.");
    drawFooter("Hold L=Check  R=Back");
    return;
  }

  int alertH = 0;
  if (highRiskAlert_) {
    alertH = 11;
    displayDriver.fillRect(MARGIN_L, CONTENT_Y + 1, SCREEN_W - MARGIN_L - 14, 10, COL_BG);
    drawClippedText(MARGIN_L, CONTENT_Y + 1, "! HIGH RISK: Open network", SCREEN_W - MARGIN_L - 18, 1, COL_DANGER, COL_BG);
    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setCursor(SCREEN_W - 12, CONTENT_Y + 1);
    displayDriver.print("x");
  }

  int itemH = ITEM_H_EXPERT;
  int listTop = CONTENT_Y + alertH;
  int listH = CONTENT_H - alertH - STATUS_H;
  int visible = listH / itemH;
  if (visible < 1) visible = 1;

  if (listIndex_ >= filteredCount_) listIndex_ = filteredCount_ - 1;
  if (listScroll_ > listIndex_) listScroll_ = listIndex_;
  if (listScroll_ < listIndex_ - visible + 1) listScroll_ = listIndex_ - visible + 1;
  if (listScroll_ < 0) listScroll_ = 0;

  int colSSID = MARGIN_L + SEL_BAR_W + 2;
  int colAuth = 110, colRSSI = 148, colRisk = 178, colCh = 210, colBadge = 228;

  displayDriver.setTextSize(1);
  for (int i = 0; i < visible; i++) {
    int idx = listScroll_ + i;
    if (idx >= filteredCount_) break;
    const NetworkRecord* n = getNetworkAtDisplayIndex(idx);
    if (!n) continue;
    int y = listTop + i * itemH;
    bool sel = (idx == listIndex_);
    int textY = y + 5;

    if (sel) displayDriver.fillRect(0, y, SEL_BAR_W, itemH - 1, COL_INFO);

    char ssidBuf[17];
    truncSSID(n->hidden ? "(hidden)" : n->ssid, ssidBuf, 16);
    displayDriver.setTextColor(n->possibleEvilTwin ? COL_DANGER : COL_FG, COL_BG);
    displayDriver.setCursor(colSSID, textY);
    displayDriver.print(ssidBuf);

    displayDriver.setTextColor(authColor(n->auth), COL_BG);
    displayDriver.setCursor(colAuth, textY);
    displayDriver.print(authStr(n->auth));

    drawSignalBars(colRSSI - 14, textY - 1, n->rssi);
    char rssiBuf[6];
    snprintf(rssiBuf, sizeof(rssiBuf), "%d", n->rssi);
    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setCursor(colRSSI, textY);
    displayDriver.print(rssiBuf);

    char riskBuf[6];
    snprintf(riskBuf, sizeof(riskBuf), "R:%u", (unsigned)n->riskScore);
    displayDriver.setTextColor(riskColor(n->riskScore), COL_BG);
    displayDriver.setCursor(colRisk, textY);
    displayDriver.print(riskBuf);

    char chBuf[5];
    snprintf(chBuf, sizeof(chBuf), "c%u", (unsigned)n->channel);
    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setCursor(colCh, textY);
    displayDriver.print(chBuf);

    if (n->newThisScan) {
      displayDriver.setTextColor(COL_INFO, COL_BG);
      displayDriver.setCursor(colBadge, textY);
      displayDriver.print("N");
    }
    if (n->possibleEvilTwin) {
      displayDriver.setTextColor(COL_DANGER, COL_BG);
      displayDriver.setCursor(colBadge + (n->newThisScan ? 7 : 0), textY);
      displayDriver.print("!");
    }

    displayDriver.drawLine(colSSID, y + itemH - 1, SCREEN_W - 4, y + itemH - 1, COL_CHROME);
  }

  if (filteredCount_ > visible) {
    int trackH = listH;
    int thumbH = trackH * visible / filteredCount_;
    if (thumbH < 6) thumbH = 6;
    int scrollRange = filteredCount_ - visible;
    int thumbY = listTop + (trackH - thumbH) * listScroll_ / (scrollRange > 0 ? scrollRange : 1);
    displayDriver.fillRect(SCREEN_W - 2, listTop, 2, trackH, COL_CHROME);
    displayDriver.fillRect(SCREEN_W - 2, thumbY, 2, thumbH, COL_DIM);
  }

  if (toast_[0]) {
    int toastY = FTR_Y - 12;
    displayDriver.fillRect(0, toastY, SCREEN_W, 11, COL_WARN);
    drawClippedText(MARGIN_L, toastY + 1, toast_, TOAST_MAX, 1, COL_BG, COL_WARN);
  }

  int statusY = CONTENT_Y + CONTENT_H - STATUS_H;
  displayDriver.fillRect(0, statusY, SCREEN_W, STATUS_H, COL_BG);
  displayDriver.setTextColor(COL_DIM, COL_BG);
  char statusBuf[48];
  uint16_t openC = 0;
  for (uint16_t i = 0; i < scan->networkCount; i++) {
    if (scan->networks[i].auth == AUTH_OPEN) openC++;
  }
  snprintf(statusBuf, sizeof(statusBuf), "%d nets Open:%u %s",
           filteredCount_, openC, sortModeStr(settings.get().sortMode));
  drawClippedText(MARGIN_L, statusY + 2, statusBuf, SCREEN_W - MARGIN_L - 6, 1, COL_DIM, COL_BG);

  drawFooter("L=Next Hold L=Info R=Back Hold R=Menu");
}

// ═══════════════════════════════════════════════════════════════════════════
//  DETAIL — SIMPLE MODE (verdict-first layout)
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawDetailSimple() {
  const NetworkRecord* n = getNetworkAtDisplayIndex(listIndex_);
  drawHeader("Network Info");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);

  if (!n) {
    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setTextSize(1);
    displayDriver.setCursor(MARGIN_L, CONTENT_Y + 20);
    displayDriver.print("No network selected.");
    drawFooter("R=Back");
    return;
  }

  UserVerdict v = getUserVerdict(*n);
  uint16_t vc = getVerdictColor(v);
  const char* vTitle = getVerdictTitle(v);

  // ── Left panel: verdict (68px) — use badge that fits ──
  int panelW = 68;
  displayDriver.drawLine(panelW, CONTENT_Y, panelW, FTR_Y - 1, COL_DIM);

  const char* badgeStr = fitBadgeText(v, panelW - 8);
  int vPixW = measureTextWidth(badgeStr, 1);
  displayDriver.setTextSize(1);
  displayDriver.setTextColor(vc, COL_BG);
  displayDriver.setCursor((panelW - vPixW) / 2, CONTENT_Y + 8);
  displayDriver.print(badgeStr);

  char scoreBuf[4];
  snprintf(scoreBuf, sizeof(scoreBuf), "%u", (unsigned)n->riskScore);
  int scorePixW = strlen(scoreBuf) * 12;
  displayDriver.setTextSize(2);
  displayDriver.setTextColor(vc, COL_BG);
  displayDriver.setCursor((panelW - scorePixW) / 2, CONTENT_Y + 32);
  displayDriver.print(scoreBuf);

  int gaugeX = 6, gaugeW = panelW - 12, gaugeY = CONTENT_Y + 54;
  displayDriver.fillRect(gaugeX, gaugeY, gaugeW, 5, COL_CHROME);
  int fillW = (int)n->riskScore * gaugeW / 100;
  if (fillW > 0) displayDriver.fillRect(gaugeX, gaugeY, fillW, 5, vc);

  const char* authSimple = (n->auth == AUTH_OPEN) ? "No password" : "Password";
  int authPixW = strlen(authSimple) * 6;
  displayDriver.setTextSize(1);
  displayDriver.setTextColor((n->auth == AUTH_OPEN) ? COL_DANGER : COL_SAFE, COL_BG);
  displayDriver.setCursor((panelW - authPixW) / 2, CONTENT_Y + 66);
  displayDriver.print(authSimple);
  drawSignalBars((panelW - 15) / 2, CONTENT_Y + 76, n->rssi);

  // ── Right panel: explanations (clipped to DETAIL_RIGHT_W) ──
  int rx = panelW + 4;
  displayDriver.setTextSize(1);
  int y = CONTENT_Y + 2;

  drawClippedText(rx, y, n->hidden ? "(name hidden)" : n->ssid, DETAIL_RIGHT_W, 1, COL_FG, COL_BG);
  y += 12;

  drawClippedText(rx, y, getVerdictReason(*n), DETAIL_RIGHT_W, 1, COL_DIM, COL_BG);
  y += 12;

  const char* reasons[4];
  int reasonCount = 0;
  getSimpleReasons(*n, reasons, reasonCount, 2);
  for (int i = 0; i < reasonCount; i++) {
    static char line[48];
    snprintf(line, sizeof(line), "- %s", reasons[i]);
    drawClippedText(rx, y, line, DETAIL_RIGHT_W, 1, COL_FG, COL_BG);
    y += 11;
  }

  if (v == VERDICT_LOGIN_REQUIRED && n->portalUrl[0]) {
    y += 2;
    drawClippedText(rx, y, "Open sign-in page on phone", DETAIL_RIGHT_W, 1, COL_INFO, COL_BG);
    y += 11;
    if (n->portalDomain[0]) {
      static char domLine[56];
      snprintf(domLine, sizeof(domLine), "Portal domain: %s", n->portalDomain);
      drawClippedText(rx, y, domLine, DETAIL_RIGHT_W, 1, COL_DIM, COL_BG);
      y += 11;
    }
    // Safety bullets: + = positive, ! = warning (ASCII-safe for small display)
    int bullets = 0;
    if (!n->portalIsIP && n->portalDomain[0]) {
      drawClippedText(rx, y, "+ Domain detected", DETAIL_RIGHT_W, 1, COL_SAFE, COL_BG);
      y += 11; bullets++;
    }
    if (n->portalPathLooksLikePortal) {
      drawClippedText(rx, y, "+ Looks like portal", DETAIL_RIGHT_W, 1, COL_SAFE, COL_BG);
      y += 11; bullets++;
    }
    if (!n->portalIsHTTPS) {
      drawClippedText(rx, y, "! Unencrypted", DETAIL_RIGHT_W, 1, COL_WARN, COL_BG);
      y += 11; bullets++;
    }
    if (n->portalBrandMismatch) {
      drawClippedText(rx, y, "! Brand mismatch", DETAIL_RIGHT_W, 1, COL_WARN, COL_BG);
      y += 11; bullets++;
    }
    if (n->portalLongUrl) {
      drawClippedText(rx, y, "! Long/suspicious URL", DETAIL_RIGHT_W, 1, COL_WARN, COL_BG);
      y += 11; bullets++;
    }
    if (bullets == 0) y += 2;
    drawClippedText(rx, y, "Don't enter passwords on sign-in", DETAIL_RIGHT_W, 1, COL_WARN, COL_BG);
    y += 14;
    // Recommendation stays above footer
    int recY = CONTENT_Y + CONTENT_H - 14;
    if (y > recY - 4) recY = y + 4;
    if (recY > FTR_Y - 13) recY = FTR_Y - 13;
    displayDriver.fillRect(rx, recY, DETAIL_RIGHT_W + 4, 11, COL_BG);
    static char recBuf[56];
    snprintf(recBuf, sizeof(recBuf), "> %s", getUserRecommendation(*n));
    drawClippedText(rx, recY, recBuf, DETAIL_RIGHT_W, 1, vc, COL_BG);
    drawFooter("Hold L=Show QR   R=Back");
    return;
  } else if (v == VERDICT_LOGIN_REQUIRED) {
    y += 2;
    drawPortalAssist(rx, y, n->portalUrl[0] ? n->portalUrl : nullptr);
    y += 34;
  }

  if (n->auth == AUTH_OPEN && v != VERDICT_LOGIN_REQUIRED) {
    drawClippedText(rx, y, "Tip: Use VPN. Don't enter passwords here.", DETAIL_RIGHT_W, 1, COL_WARN, COL_BG);
    y += 11;
  }

  int recY = CONTENT_Y + CONTENT_H - 14;
  if (y > recY - 4) recY = y + 4;
  if (recY > FTR_Y - 13) recY = FTR_Y - 13;
  displayDriver.fillRect(rx, recY, DETAIL_RIGHT_W + 4, 11, COL_BG);
  static char recBuf[56];
  snprintf(recBuf, sizeof(recBuf), "> %s", getUserRecommendation(*n));
  drawClippedText(rx, recY, recBuf, DETAIL_RIGHT_W, 1, vc, COL_BG);

  if (n->auth == AUTH_OPEN && n->grade == GRADE_UNTESTED)
    drawFooter("Hold L=Test     R=Back");
  else if (n->auth == AUTH_OPEN)
    drawFooter("Hold L=Retest   R=Back");
  else
    drawFooter("L=Next  Hold L=Select  R=Back");
}

// ═══════════════════════════════════════════════════════════════════════════
//  DETAIL — EXPERT MODE (full technical data)
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawDetailExpert() {
  const NetworkRecord* n = getNetworkAtDisplayIndex(listIndex_);
  drawHeader("Network Detail");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);

  if (!n) {
    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setTextSize(1);
    displayDriver.setCursor(MARGIN_L, CONTENT_Y + 20);
    displayDriver.print("No network selected.");
    drawFooter("R=Back");
    return;
  }

  int panelW = 68;
  displayDriver.drawLine(panelW, CONTENT_Y, panelW, FTR_Y - 1, COL_DIM);

  char scoreBuf[4];
  snprintf(scoreBuf, sizeof(scoreBuf), "%u", (unsigned)n->riskScore);
  int scoreW = strlen(scoreBuf) * 18;
  displayDriver.setTextSize(3);
  displayDriver.setTextColor(riskColor(n->riskScore), COL_BG);
  displayDriver.setCursor((panelW - scoreW) / 2, CONTENT_Y + 10);
  displayDriver.print(scoreBuf);

  RiskLabel rl = riskEngine.getRiskLabel(n->riskScore);
  const char* rlStr = (rl == RISK_HIGH) ? "HIGH" : (rl == RISK_MED) ? "MED" : "LOW";
  int labelW = strlen(rlStr) * 6;
  displayDriver.setTextSize(1);
  displayDriver.setTextColor(riskColor(n->riskScore), COL_BG);
  displayDriver.setCursor((panelW - labelW) / 2, CONTENT_Y + 40);
  displayDriver.print(rlStr);

  int gaugeX = 6, gaugeW = panelW - 12, gaugeY = CONTENT_Y + 54;
  displayDriver.fillRect(gaugeX, gaugeY, gaugeW, 5, COL_CHROME);
  int fillW = (int)n->riskScore * gaugeW / 100;
  if (fillW > 0) displayDriver.fillRect(gaugeX, gaugeY, fillW, 5, riskColor(n->riskScore));

  displayDriver.setTextColor(authColor(n->auth), COL_BG);
  int authW = strlen(authStr(n->auth)) * 6;
  displayDriver.setCursor((panelW - authW) / 2, CONTENT_Y + 66);
  displayDriver.print(authStr(n->auth));

  drawSignalBars((panelW - 15) / 2, CONTENT_Y + 76, n->rssi);
  char sigBuf[8];
  snprintf(sigBuf, sizeof(sigBuf), "%ddBm", n->rssi);
  int sigW = strlen(sigBuf) * 6;
  displayDriver.setTextColor(COL_DIM, COL_BG);
  displayDriver.setCursor((panelW - sigW) / 2, CONTENT_Y + 78);
  displayDriver.print(sigBuf);

  int rx = panelW + 4;
  displayDriver.setTextSize(1);
  int y = CONTENT_Y + 2;
  char buf[56];

  drawClippedText(rx, y, n->hidden ? "(hidden)" : n->ssid, EXPERT_DETAIL_RIGHT_W, 1, COL_FG, COL_BG);
  y += 11;

  snprintf(buf, sizeof(buf), "ch%u", (unsigned)n->channel);
  if (n->vendor[0]) { strncat(buf, " ", sizeof(buf)-strlen(buf)-1); strncat(buf, n->vendor, sizeof(buf)-strlen(buf)-1); }
  if (n->rssiLastScan > -128) {
    int trend = (int)n->rssi - (int)n->rssiLastScan;
    char tbuf[12];
    snprintf(tbuf, sizeof(tbuf), " %s%ddB", trend >= 0 ? "+" : "", trend);
    strncat(buf, tbuf, sizeof(buf)-strlen(buf)-1);
  }
  drawClippedText(rx, y, buf, EXPERT_DETAIL_RIGHT_W, 1, COL_DIM, COL_BG);
  y += 11;

  if (n->possibleEvilTwin) {
    drawClippedText(rx, y, "WARNING: evil twin!", EXPERT_DETAIL_RIGHT_W, 1, COL_DANGER, COL_BG);
    y += 10;
  }
  if (n->duplicateClass >= DUP_SAME_SSID) {
    snprintf(buf, sizeof(buf), "Dup: %u same SSID", (unsigned)n->sameSSIDCount);
    drawClippedText(rx, y, buf, EXPERT_DETAIL_RIGHT_W, 1, COL_WARN, COL_BG);
    y += 10;
  }
  if (n->riskReasonsBitmask) {
    buf[0] = '\0';
    int cnt = 0;
    for (int r = 0; r < RISK_REASON_COUNT && cnt < 3; r++) {
      if (n->riskReasonsBitmask & (1 << r)) {
        if (cnt) strcat(buf, ", ");
        const char* rs = riskEngine.getReasonString((RiskReasonId)r);
        if (strlen(buf) + strlen(rs) < sizeof(buf) - 1) strcat(buf, rs);
        cnt++;
      }
    }
    drawClippedText(rx, y, buf, EXPERT_DETAIL_RIGHT_W, 1, riskColor(n->riskScore), COL_BG);
    y += 10;
  }

  if (n->grade != GRADE_UNTESTED && n->grade != GRADE_PROTECTED) {
    const char* gradeStr = (n->grade == GRADE_FAST || n->grade == GRADE_NORMAL) ? "Internet: Working" :
      (n->grade == GRADE_SLOW) ? "Internet: Slow" :
      (n->grade == GRADE_PORTAL) ? "Portal: Login req!" :
      (n->grade == GRADE_OFFLINE) ? "Internet: None" : "Connect: Failed";
    drawClippedText(rx, y, gradeStr, EXPERT_DETAIL_RIGHT_W, 1, gradeColor(n->grade), COL_BG);
    y += 10;

    if (n->portalResult != PORTAL_NORMAL && n->portalResult != PORTAL_UNKNOWN) {
      drawClippedText(rx, y, portalStr(n->portalResult), EXPERT_DETAIL_RIGHT_W, 1, COL_WARN, COL_BG);
      y += 10;
    }
    if (n->benchmarkAvgMs >= 0) {
      snprintf(buf, sizeof(buf), "Ping:%dms Jitter:%dms", n->benchmarkAvgMs, n->benchmarkJitterMs);
      drawClippedText(rx, y, buf, EXPERT_DETAIL_RIGHT_W, 1, COL_DIM, COL_BG);
      y += 10;
    }
  }

  if (n->portalUrl[0]) {
    drawClippedText(rx, y, "Portal URL:", EXPERT_DETAIL_RIGHT_W, 1, COL_INFO, COL_BG);
    y += 10;
    drawClippedText(rx, y, n->portalUrl, EXPERT_DETAIL_RIGHT_W, 1, COL_DIM, COL_BG);
    y += 10;
    if (n->portalDomain[0]) {
      snprintf(buf, sizeof(buf), "Domain: %s", n->portalDomain);
      drawClippedText(rx, y, buf, EXPERT_DETAIL_RIGHT_W, 1, COL_DIM, COL_BG);
      y += 10;
    }
    const char* safetyStr = (n->portalSafety == PORTAL_SAFE) ? "SAFE" :
      (n->portalSafety == PORTAL_CAUTION) ? "CAUTION" : "SUSPICIOUS";
    snprintf(buf, sizeof(buf), "Safety: %s (%u)", safetyStr, (unsigned)n->portalSafetyScore);
    drawClippedText(rx, y, buf, EXPERT_DETAIL_RIGHT_W, 1,
      n->portalSafety == PORTAL_SAFE ? COL_SAFE : (n->portalSafety == PORTAL_CAUTION ? COL_WARN : COL_DANGER), COL_BG);
    y += 10;
  }

  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           n->bssid[0], n->bssid[1], n->bssid[2],
           n->bssid[3], n->bssid[4], n->bssid[5]);
  drawClippedText(rx, y, buf, EXPERT_DETAIL_RIGHT_W, 1, COL_DIM, COL_BG);
  y += 10;

  if (n->auth == AUTH_OPEN) {
    drawClippedText(rx, y, "Tip: Use VPN. Don't enter passwords.", EXPERT_DETAIL_RIGHT_W, 1, COL_WARN, COL_BG);
  }

  if (n->auth == AUTH_OPEN)
    drawFooter("L=Test  Hold L=Signal  R=Back");
  else
    drawFooter("Hold L=Signal          R=Back");
}

// ═══════════════════════════════════════════════════════════════════════════
//  ENVIRONMENT SUMMARY — SIMPLE MODE (area safety overview)
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawEnvSummarySimple() {
  const ScanRecord* scan = scanHistory.getCurrentScan();

  if (envPage_ != 0) { drawSessionSimple(); return; }

  drawHeader("Area Safety");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);

  if (!scan) {
    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setTextSize(1);
    displayDriver.setCursor(MARGIN_L, CONTENT_Y + 20);
    displayDriver.print("Check WiFi first.");
    drawFooter("R=Back");
    return;
  }

  displayDriver.setTextSize(1);
  int y = CONTENT_Y + 2;
  char buf[40];

  uint16_t suspicious = 0;
  for (uint16_t i = 0; i < scan->networkCount; i++) {
    if (scan->networks[i].possibleEvilTwin || scan->networks[i].duplicateClass >= DUP_SUSPICIOUS)
      suspicious++;
  }

  const char* areaVerdict;
  uint16_t areaColor;
  if (suspicious > 0) { areaVerdict = "RISKY AREA"; areaColor = COL_DANGER; }
  else if (scan->openCount > scan->networkCount / 2) { areaVerdict = "CAUTION"; areaColor = COL_WARN; }
  else if (scan->openCount > 0) { areaVerdict = "MOSTLY SAFE"; areaColor = COL_SAFE; }
  else { areaVerdict = "SAFE AREA"; areaColor = COL_SAFE; }

  drawClippedText(MARGIN_L, y, areaVerdict, CONTENT_W, 2, areaColor, COL_BG);
  y += 22;

  displayDriver.setTextSize(1);

  snprintf(buf, sizeof(buf), "%u networks nearby", (unsigned)scan->networkCount);
  drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_FG, COL_BG);
  y += ROW_H;

  if (scan->openCount > 0) {
    snprintf(buf, sizeof(buf), "%u without password", (unsigned)scan->openCount);
    drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_WARN, COL_BG);
    y += ROW_H;
  }

  if (suspicious > 0) {
    snprintf(buf, sizeof(buf), "%u suspicious", suspicious);
    drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_DANGER, COL_BG);
    y += ROW_H;
  }

  if (scan->portalDetectedCount > 0) {
    snprintf(buf, sizeof(buf), "%u need sign-in", (unsigned)scan->portalDetectedCount);
    drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_INFO, COL_BG);
    y += ROW_H;
  }

  const char* crowd = (scan->networkCount <= 5) ? "Low" :
                      (scan->networkCount <= 15) ? "Medium" : "High";
  snprintf(buf, sizeof(buf), "WiFi crowding: %s", crowd);
  drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_DIM, COL_BG);
  y += ROW_H;

  if (scan->safestNetworkIndex < scan->networkCount) {
    const NetworkRecord* best = &scan->networks[scan->safestNetworkIndex];
    snprintf(buf, sizeof(buf), "Best: %s", best->hidden ? "?" : best->ssid);
    displayDriver.setTextColor(COL_SAFE, COL_BG);
    drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_SAFE, COL_BG);
  }

  drawFooter("L=Next  Hold R=Menu  R=Back");
}

// ─── Session Summary — Simple Mode ──────────────────────────────────────────

void UI::drawSessionSimple() {
  drawHeader("Session");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  displayDriver.setTextSize(1);
  int y = CONTENT_Y + 4;
  char buf[40];

  SessionStats sess;
  sessionStatsGet(sess);

  snprintf(buf, sizeof(buf), "Checks performed: %lu", (unsigned long)sess.scansPerformed);
  drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_FG, COL_BG);
  y += ROW_H + 2;

  const char* safetyStr;
  uint16_t safetyCol;
  if (sess.avgRisk < 25)      { safetyStr = "Usually safe"; safetyCol = COL_SAFE; }
  else if (sess.avgRisk < 50) { safetyStr = "Mixed"; safetyCol = COL_WARN; }
  else                         { safetyStr = "Often risky"; safetyCol = COL_DANGER; }
  snprintf(buf, sizeof(buf), "Average safety: %s", safetyStr);
  drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, safetyCol, COL_BG);
  y += ROW_H + 2;

  snprintf(buf, sizeof(buf), "Open networks seen: %u", (unsigned)sess.openNetworksSeen);
  drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_FG, COL_BG);
  y += ROW_H;

  if (sess.portalsSeen > 0) {
    snprintf(buf, sizeof(buf), "Sign-in pages seen: %u", (unsigned)sess.portalsSeen);
    drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_FG, COL_BG);
    y += ROW_H;
  }

  if (sess.duplicateSsidsSeen > 0) {
    snprintf(buf, sizeof(buf), "Suspicious names: %u", (unsigned)sess.duplicateSsidsSeen);
    drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_WARN, COL_BG);
  }

  drawFooter("L=Next  Hold R=Menu  R=Back");
}

// ═══════════════════════════════════════════════════════════════════════════
//  ENVIRONMENT SUMMARY — EXPERT MODE (full technical data)
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawEnvSummaryExpert() {
  const ScanRecord* scan = scanHistory.getCurrentScan();
  drawHeader(envPage_ == 0 ? "Environment" : "Session Stats");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);

  if (!scan) {
    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setTextSize(1);
    displayDriver.setCursor(MARGIN_L, CONTENT_Y + 20);
    displayDriver.print("Scan first to see analysis.");
    drawFooter("R=Back");
    return;
  }

  displayDriver.setTextSize(1);
  int y = CONTENT_Y + 2;
  char buf[48];
  int col2 = 124;

  if (envPage_ == 0) {
    EnvStats env;
    environmentAnalysis.compute(*scan, env);

    snprintf(buf, sizeof(buf), "Networks: %u", (unsigned)env.totalNetworks);
    drawClippedText(MARGIN_L, y, buf, EXPERT_ENV_COL_W, 1, COL_FG, COL_BG);
    snprintf(buf, sizeof(buf), "Open:%u Enc:%u Hid:%u", (unsigned)env.openCount, (unsigned)env.encryptedCount, (unsigned)env.hiddenCount);
    drawClippedText(col2, y, buf, EXPERT_ENV_COL_W, 1, COL_FG, COL_BG);
    y += ROW_H;

    snprintf(buf, sizeof(buf), "Dup SSID: %u", (unsigned)env.duplicateSsids);
    drawClippedText(MARGIN_L, y, buf, EXPERT_ENV_COL_W, 1, COL_FG, COL_BG);
    snprintf(buf, sizeof(buf), "Portals: %u", (unsigned)env.portalDetectedCount);
    drawClippedText(col2, y, buf, EXPERT_ENV_COL_W, 1, COL_FG, COL_BG);
    y += ROW_H;

    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setCursor(MARGIN_L, y);
    displayDriver.print("-- Channels --");
    y += 11;

    snprintf(buf, sizeof(buf), "Best: c%u", (unsigned)env.bestChannel);
    drawClippedText(MARGIN_L, y, buf, EXPERT_ENV_COL_W, 1, COL_SAFE, COL_BG);
    snprintf(buf, sizeof(buf), "Worst: c%u", (unsigned)env.worstChannel);
    drawClippedText(col2, y, buf, EXPERT_ENV_COL_W, 1, COL_WARN, COL_BG);
    y += ROW_H;

    snprintf(buf, sizeof(buf), "Congestion: %u/10", (unsigned)env.overallCongestionScore);
    drawClippedText(MARGIN_L, y, buf, EXPERT_ENV_COL_W, 1, COL_FG, COL_BG);
    y += ROW_H;

    displayDriver.setTextColor(COL_DIM, COL_BG);
    displayDriver.setCursor(MARGIN_L, y);
    displayDriver.print("-- Quick Picks --");
    y += 11;

    const NetworkRecord* strong = nullptr;
    const NetworkRecord* risky = nullptr;
    int strongRssi = -128, riskyScore = 0;
    for (uint16_t i = 0; i < scan->networkCount; i++) {
      const NetworkRecord* nr = &scan->networks[i];
      if (nr->rssi > strongRssi) { strongRssi = nr->rssi; strong = nr; }
      if (nr->riskScore > (unsigned)riskyScore) { riskyScore = nr->riskScore; risky = nr; }
    }
    if (strong) {
      snprintf(buf, sizeof(buf), "Strong: %s %d", strong->hidden ? "?" : strong->ssid, strong->rssi);
      drawClippedText(MARGIN_L, y, buf, EXPERT_ENV_COL_W, 1, COL_SAFE, COL_BG);
    }
    if (risky && risky->riskScore >= 50) {
      snprintf(buf, sizeof(buf), "Risky: %s %u", risky->hidden ? "?" : risky->ssid, (unsigned)risky->riskScore);
      drawClippedText(col2, y, buf, EXPERT_ENV_COL_W, 1, COL_DANGER, COL_BG);
    }

    drawFooter("L=Session          R=Back");

  } else {
    SessionStats sess;
    sessionStatsGet(sess);

    snprintf(buf, sizeof(buf), "Scans: %lu", (unsigned long)sess.scansPerformed);
    drawClippedText(MARGIN_L, y, buf, EXPERT_ENV_COL_W, 1, COL_FG, COL_BG);
    snprintf(buf, sizeof(buf), "Avg networks: %u", (unsigned)sess.avgNetworkCount);
    drawClippedText(col2, y, buf, EXPERT_ENV_COL_W, 1, COL_FG, COL_BG);
    y += ROW_H;

    snprintf(buf, sizeof(buf), "Avg risk: %u", (unsigned)sess.avgRisk);
    drawClippedText(MARGIN_L, y, buf, EXPERT_ENV_COL_W, 1, riskColor(sess.avgRisk), COL_BG);
    snprintf(buf, sizeof(buf), "Most common ch: %u", (unsigned)sess.mostCommonChannel);
    drawClippedText(col2, y, buf, EXPERT_ENV_COL_W, 1, COL_FG, COL_BG);
    y += ROW_H;

    snprintf(buf, sizeof(buf), "Portals seen: %u", (unsigned)sess.portalsSeen);
    drawClippedText(MARGIN_L, y, buf, EXPERT_ENV_COL_W, 1, COL_FG, COL_BG);
    snprintf(buf, sizeof(buf), "Open seen: %u", (unsigned)sess.openNetworksSeen);
    drawClippedText(col2, y, buf, EXPERT_ENV_COL_W, 1, COL_FG, COL_BG);
    y += ROW_H;

    snprintf(buf, sizeof(buf), "Dup SSIDs seen: %u", (unsigned)sess.duplicateSsidsSeen);
    drawClippedText(MARGIN_L, y, buf, EXPERT_ENV_COL_W, 1, COL_FG, COL_BG);

    drawFooter("L=Environment      R=Back");
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  TESTING — SIMPLE MODE (user-friendly progress)
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawTestingSimple() {
  drawHeader("Testing");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  displayDriver.setTextSize(1);

  int y = CONTENT_Y + 4;
  const char* ssid = connectivityTest.getTestedSsid();
  if (ssid && ssid[0]) {
    static char lineBuf[32];
    snprintf(lineBuf, sizeof(lineBuf), "Checking: %s", ssid);
    drawClippedText(MARGIN_L, y, lineBuf, CONTENT_W, 1, COL_FG, COL_BG);
    y += ROW_H + 4;
  }

  uint8_t phase = connectivityTest.getPhase();
  int displayPhase;
  if (phase == 1)                     displayPhase = 0;
  else if (phase == 2)                displayPhase = 1;
  else if (phase == 3 || phase == 33) displayPhase = 2;
  else if (phase == 5)                displayPhase = 3;
  else                                displayPhase = 4;

  static const char* simplePhases[] = {
    "Connecting",
    "Checking internet",
    "Looking for sign-in",
    "Measuring quality"
  };

  for (int i = 0; i < 4; i++) {
    uint16_t col;
    const char* mark;

    if (i < displayPhase) {
      col = COL_SAFE; mark = "OK ";
    } else if (i == displayPhase && displayPhase < 4) {
      col = COL_FG;
      uint8_t dots = (millis() / 400) % 4;
      static char dotBuf[5];
      int d;
      for (d = 0; d <= (int)dots && d < 3; d++) dotBuf[d] = '.';
      dotBuf[d] = '\0';
      mark = dotBuf;
    } else {
      col = COL_DIM; mark = "   ";
    }

    displayDriver.fillRect(0, y, SCREEN_W, ROW_H + 2, COL_BG);
    displayDriver.setTextColor(col, COL_BG);
    displayDriver.setCursor(MARGIN_L, y + 2);
    displayDriver.print(mark);
    displayDriver.print("  ");
    displayDriver.print(simplePhases[i]);
    y += ROW_H + 2;
  }

  if (displayPhase >= 4) {
    y += 4;
    displayDriver.setTextColor(COL_SAFE, COL_BG);
    displayDriver.setCursor(MARGIN_L, y);
    displayDriver.print("Done! Check the result.");
  }

  drawFooter("Testing... please wait");
}

// ═══════════════════════════════════════════════════════════════════════════
//  TESTING — EXPERT MODE (technical phases)
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawTestingExpert() {
  drawHeader("Testing Network");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  displayDriver.setTextSize(1);

  int y = CONTENT_Y + 4;
  const char* ssid = connectivityTest.getTestedSsid();
  if (ssid && ssid[0]) {
    static char testLineBuf[40];
    snprintf(testLineBuf, sizeof(testLineBuf), "Testing: %s", ssid);
    drawClippedText(MARGIN_L, y, testLineBuf, CONTENT_W, 1, COL_FG, COL_BG);
    y += ROW_H + 2;
  }

  uint8_t phase = connectivityTest.getPhase();
  int displayPhase;
  if (phase == 1)                     displayPhase = 0;
  else if (phase == 2)                displayPhase = 1;
  else if (phase == 3 || phase == 33) displayPhase = 2;
  else if (phase == 5)                displayPhase = 3;
  else                                displayPhase = 4;

  static const char* phaseLabels[] = {
    "Joining WiFi",
    "Checking DNS",
    "Testing internet",
    "Speed test"
  };

  for (int i = 0; i < 4; i++) {
    char line[40];
    const char* status;
    uint16_t col;

    if (i < displayPhase) {
      status = "PASS"; col = COL_SAFE;
    } else if (i == displayPhase && displayPhase < 4) {
      uint8_t dots = (millis() / 400) % 4;
      static char dotBuf[5];
      int d;
      for (d = 0; d <= (int)dots && d < 3; d++) dotBuf[d] = '.';
      dotBuf[d] = '\0';
      status = dotBuf; col = COL_FG;
    } else {
      status = "---"; col = COL_DIM;
    }

    snprintf(line, sizeof(line), "[%d/4] %-16s %s", i + 1, phaseLabels[i], status);
    displayDriver.fillRect(0, y, SCREEN_W, ROW_H + 2, COL_BG);
    drawClippedText(MARGIN_L, y + 2, line, CONTENT_W, 1, col, COL_BG);
    y += ROW_H + 2;
  }

  if (displayPhase >= 4) {
    displayDriver.setTextColor(COL_SAFE, COL_BG);
    displayDriver.setCursor(MARGIN_L, y + 6);
    displayDriver.print("Done! Tap to see results.");
  }

  drawFooter("Testing... please wait");
}

// ═══════════════════════════════════════════════════════════════════════════
//  SETTINGS
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawSettings() {
  SettingsRecord s = settings.get();
  int opt = settingsOptionIndex_;
  drawHeader("Settings");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  displayDriver.setTextSize(1);

  const char* sortVal = sortModeStr(s.sortMode);
  const char* filterVal = filterStr(s.filterFlags);

  static char values[19][16];
  strncpy(values[0], sortVal, 15);             values[0][15] = '\0';
  strncpy(values[1], filterVal, 15);         values[1][15] = '\0';
  strncpy(values[2], s.expertMode ? "ON" : "OFF", 4);
  strncpy(values[3], s.alwaysMonitor ? "ON" : "OFF", 4);
  strncpy(values[4], s.alertOnHighRisk ? "ON" : "OFF", 4);
  strncpy(values[5], s.lowPowerMode ? "ON" : "OFF", 4);
  strncpy(values[6], s.demoMode ? "ON" : "OFF", 4);
  strncpy(values[7], s.privacyMode ? "ON" : "OFF", 4);
  strncpy(values[8], s.exportSummaryOnly ? "Summary" : "Full", 8);
  strncpy(values[9], s.diagnosticSerial ? "ON" : "OFF", 4);
  if (s.brightness <= 25) strncpy(values[10], "25%", 4);
  else if (s.brightness <= 50) strncpy(values[10], "50%", 4);
  else if (s.brightness <= 75) strncpy(values[10], "75%", 4);
  else strncpy(values[10], "100%", 5);
  values[10][15] = '\0';
  if (s.inactivityMs <= 60000) strncpy(values[11], "1 min", 6);
  else if (s.inactivityMs <= 120000) strncpy(values[11], "2 min", 6);
  else if (s.inactivityMs <= 300000) strncpy(values[11], "5 min", 6);
  else strncpy(values[11], "10 min", 7);
  values[11][15] = '\0';
  snprintf(values[12], sizeof(values[12]), "%us", (unsigned)s.monitorIntervalSec);
  snprintf(values[13], sizeof(values[13]), "%us", (unsigned)s.stabilityMonitorIntervalSec);
  strncpy(values[14], s.lowBatterySkipTest ? "ON" : "OFF", 4);
  strncpy(values[15], s.lowBatteryReduceMonitor ? "ON" : "OFF", 4);
  snprintf(values[16], sizeof(values[16]), "%u%%", (unsigned)s.lowBatteryDimPct);
  strncpy(values[17], s.highContrast ? "ON" : "OFF", 4);
  values[14][15] = values[15][15] = values[17][15] = '\0';
  strncpy(values[18], "R=Open", 7);
  values[18][15] = '\0';

  static const char* labels[] = {
    "Sort", "Filter", "Expert mode",
    "Monitor", "Alert", "Low power",
    "Demo mode", "Privacy", "Export mode",
    "Diagnostics",
    "Brightness", "Sleep", "Monitor int", "Stability int",
    "Skip test low", "Reduce mon low", "Dim at low", "High contrast",
    "Help"
  };

  static const int sectionAt[] = { 0, 3, 7, 10, 19 };
  static const char* sectionNames[] = { "Display", "Behavior", "Data", "Advanced" };

  int totalItems = 19;
  int rowH = ROW_H;
  int maxVisible = CONTENT_H / rowH;
  if (maxVisible < 1) maxVisible = 1;
  // Map option index to visual row (each section has 1 header + items)
  int selectedRow = 0;
  for (int sec = 0; sec < 4; sec++) {
    int secStart = sectionAt[sec];
    int secEnd = sectionAt[sec + 1];
    selectedRow += 1;  // section header
    if (opt >= secStart && opt < secEnd) {
      selectedRow += (opt - secStart);
      break;
    }
    selectedRow += (secEnd - secStart);
  }
  int totalRows = 4 + totalItems;  // 4 section headers + 19 options
  int scrollOff = 0;
  if (selectedRow >= maxVisible)
    scrollOff = selectedRow - maxVisible + 1;
  if (scrollOff > totalRows - maxVisible)
    scrollOff = totalRows - maxVisible;
  if (scrollOff < 0) scrollOff = 0;

  int y = CONTENT_Y + 1;
  int drawn = 0;
  int itemIdx = 0;
  const int valX = MARGIN_L + SETTINGS_LABEL_W + 4;

  for (int sec = 0; sec < 4; sec++) {
    int secStart = sectionAt[sec];
    int secEnd = sectionAt[sec + 1];

    if (itemIdx + (secEnd - secStart) + 1 <= scrollOff) {
      itemIdx += (secEnd - secStart) + 1;
      continue;
    }

    if (itemIdx >= scrollOff && drawn < maxVisible) {
      displayDriver.setTextColor(COL_DIM, COL_BG);
      displayDriver.setCursor(MARGIN_L, y + 2);
      displayDriver.print("-- ");
      displayDriver.print(sectionNames[sec]);
      displayDriver.print(" --");
      y += rowH;
      drawn++;
    }
    itemIdx++;

    for (int i = secStart; i < secEnd; i++) {
      if (itemIdx < scrollOff) { itemIdx++; continue; }
      if (drawn >= maxVisible) break;

      bool sel = (i == opt);
      uint16_t rowBg = sel ? COL_SEL : COL_BG;
      if (sel) displayDriver.fillRect(0, y, SCREEN_W, rowH, COL_SEL);
      drawClippedText(MARGIN_L + 2, y + 2, labels[i], SETTINGS_LABEL_W, 1, COL_FG, rowBg);
      drawClippedText(valX, y + 2, values[i], SETTINGS_VAL_W, 1, COL_INFO, rowBg);

      y += rowH;
      drawn++;
      itemIdx++;
    }
  }

  drawFooter("L=Next  R=Change  Hold R=Done");
}

void UI::drawHelp() {
  drawHeader("How to use");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  displayDriver.setTextSize(1);
  int y = CONTENT_Y + 2;
  drawClippedText(MARGIN_L, y, "Hold L = Check WiFi", CONTENT_W, 1, COL_FG, COL_BG);
  y += ROW_H;
  drawClippedText(MARGIN_L, y, "L = Next   R = Back", CONTENT_W, 1, COL_DIM, COL_BG);
  y += ROW_H;
  drawClippedText(MARGIN_L, y, "Hold L on list = Select", CONTENT_W, 1, COL_FG, COL_BG);
  y += ROW_H;
  drawClippedText(MARGIN_L, y, "Hold L on detail = Test", CONTENT_W, 1, COL_FG, COL_BG);
  y += ROW_H;
  drawClippedText(MARGIN_L, y, "Hold R = Menu / Settings", CONTENT_W, 1, COL_DIM, COL_BG);
  y += ROW_H;
  drawClippedText(MARGIN_L, y, "Both buttons = Export", CONTENT_W, 1, COL_DIM, COL_BG);
  y += ROW_H;
  drawClippedText(MARGIN_L, y, "List -> Detail -> Env -> Settings", CONTENT_W, 1, COL_INFO, COL_BG);
  drawFooter("R=Back");
}

// ═══════════════════════════════════════════════════════════════════════════
//  EXPORT
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawExport() {
  drawHeader("Export");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  displayDriver.setTextSize(1);

  int y = CONTENT_Y + 12;
  displayDriver.setTextColor(COL_SAFE, COL_BG);
  displayDriver.setCursor(MARGIN_L, y);
  displayDriver.print("Sent to Serial (USB)");
  y += 18;

  displayDriver.setTextColor(COL_FG, COL_BG);
  char fmtBuf[48];
  snprintf(fmtBuf, sizeof(fmtBuf), "Format: %s", settings.get().exportSummaryOnly ? "Summary" : "CSV");
  const ScanRecord* scan = scanHistory.getCurrentScan();
  if (scan) {
    char more[24];
    snprintf(more, sizeof(more), "  Networks: %u", (unsigned)scan->networkCount);
    strncat(fmtBuf, more, sizeof(fmtBuf) - strlen(fmtBuf) - 1);
    fmtBuf[sizeof(fmtBuf) - 1] = '\0';
  }
  drawClippedText(MARGIN_L, y, fmtBuf, CONTENT_W, 1, COL_FG, COL_BG);
  y += 18;

  snprintf(fmtBuf, sizeof(fmtBuf), "BSSID: %s", settings.get().bssidInExport ? "visible" : "masked");
  drawClippedText(MARGIN_L, y, fmtBuf, CONTENT_W, 1, COL_FG, COL_BG);

  drawFooter("L=Done");
}

// ═══════════════════════════════════════════════════════════════════════════
//  SLEEP
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawSleep() {
  displayDriver.fillScreen(COL_BG);
  displayDriver.setBacklight(0);
}

// ═══════════════════════════════════════════════════════════════════════════
//  STABILITY MONITOR
// ═══════════════════════════════════════════════════════════════════════════

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
extern StabilityData stabilityData;

void UI::drawStabilityMonitor() {
  drawHeader("Signal check");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  displayDriver.setTextSize(1);

  int y = CONTENT_Y + 3;
  char buf[56];

  snprintf(buf, sizeof(buf), "Network: %s", stabilityData.ssid[0] ? stabilityData.ssid : "(hidden)");
  drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_FG, COL_BG);
  y += ROW_H;

  if (stabilityData.sampleCount > 0) {
    int8_t latest = stabilityData.samples[stabilityData.sampleCount - 1];
    int variance = stabilityData.maxRssi - stabilityData.minRssi;

    const char* strengthNow = (latest > -50) ? "Strong" : (latest > -70) ? "OK" : "Weak";
    uint16_t strengthCol = (latest > -50) ? COL_SAFE : (latest > -70) ? COL_WARN : COL_DANGER;
    snprintf(buf, sizeof(buf), "Right now: %s", strengthNow);
    drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, strengthCol, COL_BG);
    y += ROW_H;

    snprintf(buf, sizeof(buf), "Progress: %u of %d checks", (unsigned)stabilityData.sampleCount, (int)STABILITY_MAX_SAMPLES);
    drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_DIM, COL_BG);
    y += ROW_H;

    int graphY = y;
    int graphH = 32;
    int barW = (SCREEN_W - 2 * MARGIN_L) / STABILITY_MAX_SAMPLES;
    if (barW > 22) barW = 22;

    for (uint8_t i = 0; i < stabilityData.sampleCount; i++) {
      int8_t rssi = stabilityData.samples[i];
      int barH = (rssi + 100) * graphH / 70;
      if (barH < 2) barH = 2;
      if (barH > graphH) barH = graphH;

      uint16_t barCol;
      if (rssi > -50)      barCol = COL_SAFE;
      else if (rssi > -70) barCol = COL_WARN;
      else                 barCol = COL_DANGER;

      int bx = MARGIN_L + i * barW;
      displayDriver.fillRect(bx, graphY + graphH - barH, barW - 2, barH, barCol);
    }
    y = graphY + graphH + 4;

    const char* resultLabel;
    const char* resultTip;
    uint16_t resultCol;
    if (variance <= 6) {
      resultLabel = "Steady";
      resultTip = "Good for use.";
      resultCol = COL_SAFE;
    } else if (variance <= 15) {
      resultLabel = "Some variation";
      resultTip = "Usually fine.";
      resultCol = COL_WARN;
    } else {
      resultLabel = "Very jumpy";
      resultTip = "May drop or be slow.";
      resultCol = COL_DANGER;
    }
    drawClippedText(MARGIN_L, y, resultLabel, CONTENT_W, 1, resultCol, COL_BG);
    y += ROW_H;
    drawClippedText(MARGIN_L, y, resultTip, CONTENT_W, 1, COL_DIM, COL_BG);
  } else {
    drawClippedText(MARGIN_L, y, stabilityData.scanning ? "Checking signal strength..." : "Starting signal check...", CONTENT_W, 1, COL_DIM, COL_BG);
    y += ROW_H;
    drawClippedText(MARGIN_L, y, "Hold R to cancel.", CONTENT_W, 1, COL_DIM, COL_BG);
  }

  drawFooter("R = Stop");
}

// ═══════════════════════════════════════════════════════════════════════════
//  DEBUG
// ═══════════════════════════════════════════════════════════════════════════

void UI::drawDebug() {
  drawHeader("Debug");
  displayDriver.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
  displayDriver.setTextSize(1);
  displayDriver.setTextColor(COL_WARN, COL_BG);
  displayDriver.setCursor(MARGIN_L, CONTENT_Y + 3);
  displayDriver.print("Manufacturing Mode");

  displayDriver.setTextColor(COL_FG, COL_BG);
  char buf[48];
  int y = CONTENT_Y + 18;
  const int col2 = MARGIN_L + DEBUG_LEFT_W + 4;

#ifdef ESP32
  snprintf(buf, sizeof(buf), "Chip: %s r%d", ESP.getChipModel(), ESP.getChipRevision());
  drawClippedText(MARGIN_L, y, buf, DEBUG_LEFT_W, 1, COL_FG, COL_BG);
  snprintf(buf, sizeof(buf), "Heap: %lu free", (unsigned long)ESP.getFreeHeap());
  drawClippedText(col2, y, buf, DEBUG_RIGHT_W, 1, COL_FG, COL_BG);
  y += ROW_H;

  snprintf(buf, sizeof(buf), "Flash: %luKB", (unsigned long)(ESP.getFlashChipSize() / 1024));
  drawClippedText(MARGIN_L, y, buf, DEBUG_LEFT_W, 1, COL_FG, COL_BG);
  snprintf(buf, sizeof(buf), "Sketch: %luKB", (unsigned long)(ESP.getSketchSize() / 1024));
  drawClippedText(col2, y, buf, DEBUG_RIGHT_W, 1, COL_FG, COL_BG);
  y += ROW_H;
#endif

  snprintf(buf, sizeof(buf), "Uptime: %lus", (unsigned long)(millis() / 1000));
  drawClippedText(MARGIN_L, y, buf, DEBUG_LEFT_W, 1, COL_FG, COL_BG);
  snprintf(buf, sizeof(buf), "Bat: %u%% %.2fV", (unsigned)powerManager.getBatteryPct(), powerManager.getBatteryVoltage());
  drawClippedText(col2, y, buf, DEBUG_RIGHT_W, 1, COL_FG, COL_BG);
  y += ROW_H;

  bool b1 = (digitalRead(PIN_BTN1) == LOW);
  bool b2 = (digitalRead(PIN_BTN2) == LOW);
  snprintf(buf, sizeof(buf), "BTN1: %s  BTN2: %s", b1 ? "ON" : "---", b2 ? "ON" : "---");
  drawClippedText(MARGIN_L, y, buf, CONTENT_W, 1, COL_FG, COL_BG);

  drawFooter("R=Exit  Hold R=Factory reset");
}
