#include <Arduino.h>
#include "Blinker.h"
#include "ProMini.h"

Blinker::Blinker() : _ms(500), _lastMark(0), _state(false), _enabled(false), _count(0) {}

void Blinker::message(unsigned long ms, int count) {
  _count = count << 1;
  enable(ms);
}

void Blinker::enable(unsigned long ms) {
  _state = true;
  _enabled = true;
  _lastMark = millis();
  _ms = ms;
  LED_SETHIGH();
}

void Blinker::disable() {
  _state = false;
  _enabled = false;
  LED_SETLOW();
}

void Blinker::update() {
  if (!_enabled) {
    return;
  }

  if (millis() - _lastMark > _ms) {
    _lastMark = millis();
    _state = !_state;
    LED_SET(_state ? HIGH : LOW);
    if (_count) {
      _count--;
      if (!_count) {
        disable();
      }
    }
  }
}
