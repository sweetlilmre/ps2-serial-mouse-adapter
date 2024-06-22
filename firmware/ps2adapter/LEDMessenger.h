#pragma once

class LEDMessenger
{
public:
  LEDMessenger();
  ~LEDMessenger();
  void start();
  void push(int delay, int repeat, int nextMessageDelay);

private:
  int m_timerResolution;
  int m_nextMessageDelay;
  static LEDMessenger *instance;

  struct LEDMessage {
    byte state;
    int repeat;
    int stepDelay;
    int curStepDelay;
    int nextMessageDelay;
  };

  typedef struct node
  {
    LEDMessage item;
    node *next;
  } node;

  node *list;
  int length;

  void update();
  static void TimerStatic();
  void pop();
  LEDMessage* get();
  int size();
};
