#pragma once

class Blinker {
  public:
    Blinker();
    void setTiming(long ms);
    void enable();
    void disable();
    void update();
  private:
    unsigned long _ms, _lastMark;
    bool _state;
    bool _enabled;
};