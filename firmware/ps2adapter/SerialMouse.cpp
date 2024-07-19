#include "ProMini.h"
#include "SerialMouse.h"

static const int RS232_RTS = 3;
static SerialMouse* classPtr = NULL;
// set compare match register for 1200.0300007500186 Hz increments = 16000000 / (1 * 1200.0300007500186) - 1 (must be <65536)
#define COMPA_1200_BAUD 13332
#define UART_TX_DATA_BITS 7
static bool txSendByteState = false;
static uint8_t txBuffer[256];
static volatile uint8_t txBufferHead, txBufferTail = 0;


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

void  SerialMouse::sendByte(uint8_t data) {
  txBuffer[txBufferHead++] = data;
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


ISR(TIMER1_COMPA_vect)
{
  static uint8_t txDataByte = 0;
  static uint8_t txDataBits = 0;  

  if (!txSendByteState) {
    // if there is data to transmit
    if (txBufferTail != txBufferHead) {
      // switch state to byte transmission
      txSendByteState = true;
      // get the byte and set the byte width (7 for our case)
      txDataByte = txBuffer[txBufferTail++];
      txDataBits = UART_TX_DATA_BITS;
      // Transmit the Start bit
      // reset the overflow timer for next bit
      RS_SETTXLOW();
    }
  } else {
    // start bit is done, count down the data bits
    if (txDataBits) {
      txDataBits--;
      uint8_t bit = txDataByte & 0x01;
      txDataByte >>= 1;
      // reset the overflow timer for next bit
      RS_SETTX(bit);
    } else {
      // if all data bits are transmitted, send stop bit and go idle until next byte
      txSendByteState = false;
      RS_SETTXHIGH();
    }
  }
}


void SerialMouse::init() {
  TIMSK1 &= ~(1 << OCIE1A); // Disable timer compare interrupt
  Serial.println("Starting serial port");
  RS_SETTXHIGH();

  memset(txBuffer, 0, sizeof(txBuffer));
  txBufferHead = txBufferTail = 0;
  txSendByteState = false;

  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1  = 0; // initialize counter value to 0
  OCR1A = COMPA_1200_BAUD; // 1200.0300007500186 BAUD
  TCCR1B |= (1 << WGM12); // turn on CTC mode
  TCCR1B |= (0 << CS12) | (0 << CS11) | (1 << CS10); // Set CS12, CS11 and CS10 bits for 1 prescaler
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
  
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
