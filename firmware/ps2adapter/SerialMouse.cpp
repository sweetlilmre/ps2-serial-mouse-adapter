#include "ProMini.h"
#include "SerialMouse.h"

static const int RS232_RTS = 3;
static SerialMouse* classPtr = NULL;

SerialMouse* SerialMouse::instance(mouseType_t mouseType) {
  if (classPtr == NULL) {
    classPtr = new SerialMouse(mouseType);
  }
  return classPtr;
}


void SerialMouse::RTSInterruptStatic() {
  if (classPtr != NULL) {
    // signal the main loop to ignore the PS/2 mouse data until the serial port is ready
    classPtr->serialInitDone = false;
    // signal the main loop to initialize the serial port
    classPtr->doSerialInit = true;
  }
}

SerialMouse::SerialMouse(mouseType_t mouseType) {
  RS_DIRTXOUT();
  JP12_DIRIN_UP();
  JP34_DIRIN_UP();
  this->mouseType = mouseType;

  twoButtons = (JP12_READ() == LOW);
  wheelMouse = (JP34_READ() == LOW);
  doSerialInit = true;
  serialInitDone = false;

  Serial.println("Listening on RTS");
  attachInterrupt(digitalPinToInterrupt(RS232_RTS), RTSInterruptStatic, FALLING);
}

void SerialMouse::sendBit(int data) {
  // Delay between the signals to match 1200 baud
  static const auto usDelay = 1000000 / 1200;
  RS_SETTX(data);
  delayMicroseconds(usDelay);
}

void SerialMouse::sendByte(byte data) {

  // Start bit
  sendBit(0);

  // Data bits
  for (int i = 0; i < 7; i++) {
    sendBit((data >> i) & 0x01);
  }

  // Stop bit
  sendBit(1);

  // 7+1 bits is normal mouse protocol, but some serial controllers
  // expect 8+1 bits format. We send additional stop bit to stay
  // compatible to that kind of controllers.
  sendBit(1);
}

void SerialMouse::send(const Ps2Report_t& data) {

  auto dx = constrain(data.x, -127, 127);
  auto dy = constrain(-data.y, -127, 127);
  byte lb = (data.buttons & MB_LEFT) ? 0x20 : 0;
  byte rb = (data.buttons & MB_RIGHT) ? 0x10 : 0;
  sendByte(0x40 | lb | rb | ((dy >> 4) & 0xC) | ((dx >> 6) & 0x3));
  sendByte(dx & 0x3F);
  sendByte(dy & 0x3F);
  if (!twoButtons) {
    byte mb;
    if (wheelMouse) {
      // according to: https://sourceforge.net/p/cutemouse/trunk/ci/master/tree/cutemouse/PROTOCOL.TXT
      // for a wheel mouse, the middle button should be reported in bit 0x10
      mb = (data.buttons & MB_MIDDLE) ? 0x10 : 0;
      mb |= (data.wheel & 0x0F);
    } else {
      // for a 3-button mouse, the middle button should be reported in bit 0x20
      mb = (data.buttons & MB_MIDDLE) ? 0x20 : 0;
    }
    sendByte(mb);
  }
}

void SerialMouse::init() {

  Serial.println("Starting serial port");
  RS_SETTXHIGH();
  delayMicroseconds(10000);
  sendByte('M');
  if(!twoButtons) {
    // if wheelMouse mode has been set by jumper, confirm that the mouse has the capability
    if(wheelMouse && (mouseType == mouseType_t::wheelMouse)) {
      // set wheelMouse mode
      sendByte('Z');
      Serial.println("Init wheel mode");
    } else {
      // set 3-button (Logitech) mode
      sendByte('3');
      Serial.println("Init 3-button mode");
    }
  } else {
    Serial.println("Init 2-button mode");
  }
  delayMicroseconds(10000);
  // Signal the main loop that the serial port is ready
  doSerialInit = false;
  serialInitDone = true;
}
