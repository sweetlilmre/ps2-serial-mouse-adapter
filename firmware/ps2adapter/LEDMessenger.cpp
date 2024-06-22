#include <Arduino.h>
#include "LEDMessenger.h"
#include "ProMini.h"


LEDMessenger::LEDMessenger() {
  list = NULL;
  length = 0;
  m_timerResolution = 10;
  m_nextMessageDelay = 0;
};

LEDMessenger::~LEDMessenger() {
  while (list) {
    node *tmp = list;
    list = list->next;
    delete tmp;
  }
};

LEDMessenger* LEDMessenger::instance = NULL;

void LEDMessenger::TimerStatic() {
  LEDMessenger::instance->update();
};

typedef void (*voidFuncPtr)(void);
static volatile voidFuncPtr intFunc = NULL;

ISR(TIMER1_OVF_vect)
{
  TCNT1 = 45535;        // Timer Preloading
  if (intFunc != NULL)
    intFunc();
}

void LEDMessenger::start() {
  instance = this;
  intFunc = TimerStatic;
  LED_SETLOW();
  TCCR1A = 0;           // Init Timer1A
  TCCR1B = 0;           // Init Timer1B
  TCCR1B |= B00000010;  // Prescaler = 64
  TCNT1 = 45535;        // Timer Preloading
  TIMSK1 |= B00000001;  // Enable Timer Overflow Interrupt
};

void LEDMessenger::push(int delay, int repeat, int nextMessageDelay) {
  node *newNode = new node;
  newNode->item.state = HIGH;
  newNode->item.stepDelay = delay;
  newNode->item.curStepDelay = delay;
  newNode->item.repeat = repeat;
  newNode->item.nextMessageDelay = nextMessageDelay;
  newNode->next = NULL;
  if (list) {
    node *tmp = list;
    while (tmp->next) {
      tmp = tmp->next;
    }
    tmp->next = newNode;
  } else {
    list = newNode;
  }
  length++;
};

void LEDMessenger::pop() {
  if (list) {
    node *tmp = list;
    list = list->next;
    delete tmp;
    length--;
  }
};

LEDMessenger::LEDMessage* LEDMessenger::get() {
  if (list) {
    return &(list->item);
  } else {
    return NULL;
  };
};

int LEDMessenger::size() {
  return length;
};

void LEDMessenger::update() {
  if (m_nextMessageDelay > 0) {
    m_nextMessageDelay -= m_nextMessageDelay;
    return;
  }

  LEDMessenger::LEDMessage* pItem = get();
  if (pItem) {
    LED_SET(pItem->state);
    if (pItem->curStepDelay > 0) {
      pItem->curStepDelay -= m_timerResolution;
    } else {
      pItem->curStepDelay = pItem->stepDelay;
      pItem->state = HIGH - pItem->state;
      // one full HIGH -> LOW -> HIGH transistion
      if (pItem->state == HIGH) {
        // continous blink
        if (pItem->repeat < 0) {
          // waiting message
          if (size() > 1) {
            m_nextMessageDelay = pItem->nextMessageDelay;
            pop();
          }
          return;
        }
        pItem->repeat--;
        if (pItem->repeat <= 0) {
          m_nextMessageDelay = pItem->nextMessageDelay;
          pop();
        }
      }
    }
  }
}
