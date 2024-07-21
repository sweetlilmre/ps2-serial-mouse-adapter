#include <Arduino.h>
#include "MouseData.h"

typedef struct SERIAL_REPORT {
  uint8_t buttons, lastButtons;
  unsigned long lastTime;
  int8_t  x;
  int8_t  y;
  int8_t  wheel;
} SerialReport_t;



class SerialMouse {
public:

  static SerialMouse* instance(mouseType_t mouseType);
  void init();
  bool update(const Ps2Report_t& data);
  void send();
  bool requiresInit() { return doSerialInit; }
  bool isInitialized() { return serialInitDone; }

private:
  bool twoButtons = false;
  bool wheelMouse = false;
  volatile bool doSerialInit;
  volatile bool serialInitDone;
  mouseType_t mouseType;
  SerialReport_t report;
  unsigned long pktTime;


  SerialMouse(mouseType_t mouseType);
  void sendByte(uint8_t data);
  void sendBytes(const uint8_t* data, uint8_t len);
  static void RTSInterruptStatic();
};
