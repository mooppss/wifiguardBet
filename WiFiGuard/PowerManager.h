#ifndef POWERMANAGER_H
#define POWERMANAGER_H

#include "Config.h"
#include "Types.h"

class PowerManager {
public:
  void begin();
  void update();  // call every loop; handles inactivity, battery, sleep
  void resetInactivity();
  uint8_t getBatteryPct() const { return batteryPct_; }
  float getBatteryVoltage() const { return batteryVoltage_; }
  bool isLowBattery() const { return batteryPct_ < lowThreshold_; }
  bool isCriticalBattery() const { return batteryPct_ < criticalThreshold_; }

private:
  void readBattery();
  void enterSleep();

  uint32_t lastBatteryRead_;
  uint32_t lastActivity_;
  uint8_t  batteryPct_;
  float    batteryVoltage_;
  uint8_t  lowThreshold_;
  uint8_t  criticalThreshold_;
  bool     dimmed_;
};

extern PowerManager powerManager;

#endif
