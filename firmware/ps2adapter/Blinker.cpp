#include <Arduino.h>
#include "Blinker.h"
#include "ProMini.h"

Blinker::Blinker() : _ms(500), _lastMark(0), _state(false), _enabled(false) {}

void Blinker::setTiming(long ms) {
  _ms = ms;
}

void Blinker::enable() {
  _enabled = true;
  _lastMark = millis();
}

void Blinker::disable() {
  _enabled = false;
}

void Blinker::update() {
  if (!_enabled) {
    return;
  }
  
  if (millis() - _lastMark > _ms) {
    _lastMark = millis();
    _state = !_state;
    LED_SET(_state ? HIGH : LOW);
  }
}
