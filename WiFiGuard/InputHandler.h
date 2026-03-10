#ifndef INPUTHANDLER_H
#define INPUTHANDLER_H

#include "Config.h"
#include "Types.h"

class InputHandler {
public:
  void begin(uint8_t pin1 = PIN_BTN1, uint8_t pin2 = PIN_BTN2);
  void update();
  bool pollEvent(InputEvent& out);
  void flushQueue() { queueHead_ = queueTail_ = queueCount_ = 0; }

private:
  uint8_t  pin1_, pin2_;
  bool     lastRaw1_, lastRaw2_;       // raw (un-debounced) previous reading
  bool     debounced1_, debounced2_;   // debounced stable state
  uint32_t stableSince1_, stableSince2_;
  bool     longFired1_, longFired2_;
  uint32_t pressStart1_, pressStart2_;
  InputEvent queue_[INPUT_QUEUE_SIZE];
  uint8_t   queueHead_, queueTail_, queueCount_;
  bool      chordEmitted_;
  void pushEvent(InputEventType t, uint16_t durationMs = 0);
  void processButtons();
};

extern InputHandler inputHandler;

#endif
