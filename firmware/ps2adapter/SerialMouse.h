#include <Arduino.h>
#include "MouseData.h"

enum UART_TX_STATE {
  TX_IDLE,
  TX_DATA,
};


class SerialMouse {
public:

  static SerialMouse* instance(mouseType_t mouseType);
  void init();
  void send(const Ps2Report_t& data);
  bool requiresInit() { return doSerialInit; }
  bool isInitialized() { return serialInitDone; }

private:
  bool twoButtons = false;
  bool wheelMouse = false;
  volatile bool doSerialInit;
  volatile bool serialInitDone;
  mouseType_t mouseType;

  SerialMouse(mouseType_t mouseType);
  void sendByte(uint8_t data);
  static void RTSInterruptStatic();
};
