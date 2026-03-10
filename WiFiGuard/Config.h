#ifndef CONFIG_H
#define CONFIG_H

// Build-time configuration for WiFiGuard (ESP32 Arduino)

// === Display (landscape dimensions after rotation) ===
// Named SCREEN_W / SCREEN_H to avoid collision with TFT_eSPI internal macros
#define SCREEN_W    240
#define SCREEN_H    135
#define TFT_ROTATION 1  // 0-3 for TFT_eSPI

// === Pins (LilyGO T-Display / common ESP32 dev) ===
#ifndef PIN_BTN1
#define PIN_BTN1    0
#endif
#ifndef PIN_BTN2
#define PIN_BTN2    35
#endif
#ifndef PIN_BATTERY_ADC
#define PIN_BATTERY_ADC  34
#endif
#ifndef PIN_TFT_BL
#define PIN_TFT_BL   27
#endif

// === WiFi Scan ===
#define MAX_NETWORKS        25
#define MAX_RAW_NETWORKS    50  // temp buffer, must fit in static alloc
#define SCAN_TIMEOUT_MS     15000
#define WIFI_SCAN_ASYNC     1   // 1 = async (original); 0 = blocking

// === Risk engine ===
#define MAX_RISK_REASONS    8
#define RISK_SCORE_MAX      100

// === Connectivity test ===
#define CONNECT_TIMEOUT_MS  5000
#define DNS_TIMEOUT_MS      3000
#define HTTP_TIMEOUT_MS     5000
#define CONNECTIVITY_URL    "http://connectivitycheck.gstatic.com/generate_204"
#define CONNECTIVITY_URL_ALT "http://captive.apple.com/hotspot-detect.html"

// === History & persistence ===
#define HISTORY_SIZE        3    // scans in RAM circular buffer (each ~3.5KB)
#define HISTORY_NVS_MAX     3    // scans to persist to NVS
#define SESSION_ONLY_DEFAULT 0

// === Input ===
#define DEBOUNCE_MS         25
#define LONG_PRESS_MS       400
#define CHORD_WINDOW_MS     500
#define INPUT_QUEUE_SIZE    8

// === Power ===
#define BATTERY_POLL_MS     5000
#define INACTIVITY_MS       60000   // 1 min default
#define INACTIVITY_MS_MIN   30000
#define INACTIVITY_MS_MAX   300000
#define LOW_BATTERY_PCT     20
#define CRITICAL_BATTERY_PCT 10
#define BATTERY_VMIN        3.0f
#define BATTERY_VMAX        4.2f

// === UI ===
#define LIST_ITEM_HEIGHT    24
#define MAX_VISIBLE_ITEMS   8
#define REDRAW_THROTTLE_MS  100

// === Stability monitor ===
#define STABILITY_MAX_SAMPLES  10

// === Benchmark ===
#define BENCHMARK_PINGS        3

// === Anomaly ===
#define ANOMALY_NEW_THRESHOLD  5

// === Backlight PWM ===
#define BL_LEDC_CHANNEL        0

// === Feature flags (compile-time) ===
#define FEATURE_EXPORT_JSON  1
#define FEATURE_NEW_GONE     1
#define FEATURE_EXPERT_MODE  1
#define FEATURE_DIAG_SERIAL  1
#define DEBUG_SERIAL         0
// Optional: install "QRCode" by ricmoo (Arduino Library Manager), then uncomment to show portal URL as QR
// #define USE_QRCODE_LIB  1

#endif
