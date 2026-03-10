#include "StateMachine.h"

StateMachine stateMachine;

void StateMachine::begin() {
  state_ = STATE_IDLE;
  scanPhase_ = 0;
  testPhase_ = 0;
}

void StateMachine::onInputEvent(InputEventType evt) {
  switch (state_) {
    case STATE_IDLE:
      if (evt == EVT_LONG_B1) {
        state_ = STATE_SCANNING;
        scanPhase_ = 1;
      } else if (evt == EVT_LONG_B2) {
        state_ = STATE_SETTINGS;
      } else if (evt == EVT_CHORD) {
        state_ = STATE_DEBUG;
      }
      break;
    case STATE_SCANNING:
      if (evt == EVT_LONG_B2) {
        state_ = STATE_IDLE;
        scanPhase_ = 0;
      }
      break;
    case STATE_PROCESSING:
      break;
    case STATE_BROWSING:
      if (evt == EVT_CHORD) {
        state_ = STATE_EXPORT;
      }
      break;
    case STATE_TESTING:
      break;
    case STATE_EXPORT:
      if (evt == EVT_TAP_B1 || evt == EVT_TAP_B2) {
        state_ = STATE_BROWSING;
      }
      break;
    case STATE_SETTINGS:
      break;
    case STATE_SLEEP:
      if (evt != EVT_NONE) {
        state_ = STATE_IDLE;
      }
      break;
    default:
      break;
  }
}

void StateMachine::onScanComplete() {
  if (state_ == STATE_SCANNING) {
    scanPhase_ = 2;
    state_ = STATE_PROCESSING;
  }
}

void StateMachine::onProcessingComplete() {
  if (state_ == STATE_PROCESSING) {
    state_ = STATE_BROWSING;
  }
}

void StateMachine::onTestComplete() {
  if (state_ == STATE_TESTING) {
    testPhase_ = 4;
    state_ = STATE_BROWSING;
  }
}

void StateMachine::onInactivityTimeout() {
  if (state_ == STATE_IDLE || state_ == STATE_BROWSING) {
    state_ = STATE_SLEEP;
  }
}

void StateMachine::onLowBattery() {
  if (state_ != STATE_SLEEP) {
    state_ = STATE_SLEEP;
  }
}

void StateMachine::onExportComplete() {
  if (state_ == STATE_EXPORT) {
    state_ = STATE_BROWSING;
  }
}

void StateMachine::onSettingsExit() {
  if (state_ == STATE_SETTINGS) {
    state_ = STATE_IDLE;
  }
}
