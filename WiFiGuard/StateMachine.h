#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include "Types.h"

// State machine for device. Transitions driven by input and completion events.
class StateMachine {
public:
  void begin();
  DeviceState getState() const { return state_; }
  void setState(DeviceState s) { state_ = s; }
  void onInputEvent(InputEventType evt);
  void onScanComplete();
  void onProcessingComplete();
  void onTestComplete();
  void onInactivityTimeout();
  void onLowBattery();
  void onExportComplete();
  void onSettingsExit();

  // Sub-state for scanning (non-blocking)
  uint8_t getScanPhase() const { return scanPhase_; }
  void setScanPhase(uint8_t p) { scanPhase_ = p; }

  // Sub-state for connectivity test
  uint8_t getTestPhase() const { return testPhase_; }
  void setTestPhase(uint8_t p) { testPhase_ = p; }

private:
  DeviceState state_;
  uint8_t     scanPhase_;   // 0=idle, 1=started, 2=done
  uint8_t     testPhase_;   // 0=idle, 1=connect, 2=dns, 3=http, 4=done
};

extern StateMachine stateMachine;

#endif
