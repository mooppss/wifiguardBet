#include "PowerManager.h"
#include "Settings.h"
#include "StateMachine.h"
#include "DisplayDriver.h"
#include <Arduino.h>

PowerManager powerManager;

void PowerManager::begin() {
  lastBatteryRead_ = 0;
  lastActivity_ = millis();
  batteryPct_ = 100;
  batteryVoltage_ = BATTERY_VMAX;
  lowThreshold_ = LOW_BATTERY_PCT;
  criticalThreshold_ = CRITICAL_BATTERY_PCT;
  dimmed_ = false;
  SettingsRecord s = settings.get();
  if (s.lowBatteryPct) {
    lowThreshold_ = s.lowBatteryPct;
    criticalThreshold_ = s.lowBatteryPct / 2;
    if (criticalThreshold_ < 5) criticalThreshold_ = 5;
  }
}

void PowerManager::resetInactivity() {
  lastActivity_ = millis();
}

void PowerManager::readBattery() {
#ifdef PIN_BATTERY_ADC
  int raw = analogRead(PIN_BATTERY_ADC);
  const float divRatio = 2.0f;  // board-dependent: adjust for your voltage divider
  float v = (3.3f / 4095.0f) * (float)raw * divRatio;
  batteryVoltage_ = v;
  if (v <= BATTERY_VMIN) batteryPct_ = 0;
  else if (v >= BATTERY_VMAX) batteryPct_ = 100;
  else batteryPct_ = (uint8_t)((v - BATTERY_VMIN) / (BATTERY_VMAX - BATTERY_VMIN) * 100.0f);
#else
  batteryVoltage_ = 4.0f;
  batteryPct_ = 80;
#endif
}

void PowerManager::enterSleep() {
  displayDriver.setBacklight(0);
  stateMachine.setState(STATE_SLEEP);
}

void PowerManager::update() {
  uint32_t now = millis();
  if (now - lastBatteryRead_ >= BATTERY_POLL_MS) {
    readBattery();
    lastBatteryRead_ = now;
    if (isCriticalBattery()) {
      stateMachine.onLowBattery();
    } else if (isLowBattery() && !dimmed_) {
      uint8_t dim = settings.get().lowBatteryDimPct;
      if (dim > 0 && dim < settings.get().brightness) {
        displayDriver.setBacklight(dim);
        dimmed_ = true;
      }
    } else if (!isLowBattery() && dimmed_) {
      displayDriver.setBacklight(settings.get().brightness);
      dimmed_ = false;
    }
  }
  uint32_t inactivityMs = settings.get().inactivityMs;
  if (settings.get().lowPowerMode && inactivityMs > 30000) inactivityMs = 30000;
  if (inactivityMs > 0 && (now - lastActivity_ >= inactivityMs)) {
    DeviceState st = stateMachine.getState();
    if (st != STATE_SLEEP && st != STATE_SCANNING && st != STATE_TESTING) {
      stateMachine.onInactivityTimeout();
      lastActivity_ = now;  // prevent re-firing every loop
    }
  }
}
