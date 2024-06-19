#pragma once

class Blinker {
  public:
    Blinker();
    void message(unsigned long ms, int count); 
    void enable(unsigned long ms);
    void disable();
    void update();
  private:
    unsigned long _ms, _lastMark;
    bool _state;
    bool _enabled;
    int _count;
};