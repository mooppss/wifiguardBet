#include "InputHandler.h"
#include <Arduino.h>

InputHandler inputHandler;

void InputHandler::begin(uint8_t pin1, uint8_t pin2) {
  pin1_ = pin1;
  pin2_ = pin2;
  pinMode(pin1_, INPUT_PULLUP);
  pinMode(pin2_, INPUT_PULLUP);
  bool r1 = (digitalRead(pin1_) == LOW);
  bool r2 = (digitalRead(pin2_) == LOW);
  lastRaw1_ = r1;  lastRaw2_ = r2;
  debounced1_ = r1; debounced2_ = r2;
  stableSince1_ = millis();
  stableSince2_ = millis();
  longFired1_ = longFired2_ = false;
  pressStart1_ = pressStart2_ = 0;
  queueHead_ = queueTail_ = queueCount_ = 0;
  chordEmitted_ = false;
}

void InputHandler::pushEvent(InputEventType t, uint16_t durationMs) {
  if (queueCount_ >= INPUT_QUEUE_SIZE) return;
  queue_[queueTail_].type = t;
  queue_[queueTail_].durationMs = durationMs;
  queueTail_ = (queueTail_ + 1) % INPUT_QUEUE_SIZE;
  queueCount_++;
}

void InputHandler::processButtons() {
  bool raw1 = (digitalRead(pin1_) == LOW);
  bool raw2 = (digitalRead(pin2_) == LOW);
  uint32_t now = millis();

  // Reset stable timer whenever raw reading changes (bounce detection)
  if (raw1 != lastRaw1_) { lastRaw1_ = raw1; stableSince1_ = now; }
  if (raw2 != lastRaw2_) { lastRaw2_ = raw2; stableSince2_ = now; }

  // Button 1: only accept transition after signal stable for DEBOUNCE_MS
  if ((now - stableSince1_) >= (uint32_t)DEBOUNCE_MS) {
    if (raw1 != debounced1_) {
      debounced1_ = raw1;
      if (raw1) {
        pressStart1_ = now;
        longFired1_ = false;
      } else {
        if (!longFired1_) {
          pushEvent(EVT_TAP_B1, (uint16_t)(now - pressStart1_));
        }
        longFired1_ = false;
      }
    }
  }
  if (debounced1_ && !longFired1_ && (now - pressStart1_ >= LONG_PRESS_MS)) {
    longFired1_ = true;
    pushEvent(EVT_LONG_B1, (uint16_t)(now - pressStart1_));
  }

  // Button 2: same debounce logic
  if ((now - stableSince2_) >= (uint32_t)DEBOUNCE_MS) {
    if (raw2 != debounced2_) {
      debounced2_ = raw2;
      if (raw2) {
        pressStart2_ = now;
        longFired2_ = false;
      } else {
        if (!longFired2_) {
          pushEvent(EVT_TAP_B2, (uint16_t)(now - pressStart2_));
        }
        longFired2_ = false;
      }
    }
  }
  if (debounced2_ && !longFired2_ && (now - pressStart2_ >= LONG_PRESS_MS)) {
    longFired2_ = true;
    pushEvent(EVT_LONG_B2, (uint16_t)(now - pressStart2_));
  }

  // Chord: both pressed within CHORD_WINDOW_MS
  if (debounced1_ && debounced2_
      && (now - pressStart1_ < CHORD_WINDOW_MS)
      && (now - pressStart2_ < CHORD_WINDOW_MS)) {
    if (!chordEmitted_) {
      chordEmitted_ = true;
      pushEvent(EVT_CHORD);
    }
  } else {
    chordEmitted_ = false;
  }
}

void InputHandler::update() {
  processButtons();
}

bool InputHandler::pollEvent(InputEvent& out) {
  if (queueCount_ == 0) {
    out.type = EVT_NONE;
    return false;
  }
  out = queue_[queueHead_];
  queueHead_ = (queueHead_ + 1) % INPUT_QUEUE_SIZE;
  queueCount_--;
  return true;
}
