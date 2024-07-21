#include "ProMini.h"
#include "SerialMouse.h"



#define BAUD_RATE 1200
#define COMPA_1200_BAUD_BIT_VALUE 13332 // set compare match register for 1200.0300007500186 Hz increments: 16000000 / 13333 = 1200.0300007500186
#define UART_TX_DATA_BITS 7
#define USE_EXTRA_STOP_BIT 1 // necroware reports that some serial interfaces require an extra stop bit

#ifdef USE_EXTRA_STOP_BIT
  #define SERIAL_BYTE_SIZE 10  // 1 start bit, 7 data bits, 2 stop bits
#else
  #define SERIAL_BYTE_SIZE 9  // 1 start bit, 7 data bits, 1 stop bit
#endif

static const double bitDelay = 1000000 / BAUD_RATE; // Delay between the signals to match 1200 baud
static const unsigned long pktTime_2btn = (bitDelay * SERIAL_BYTE_SIZE * 3);
static const unsigned long pktTime_3btn = (bitDelay * SERIAL_BYTE_SIZE * 4);

static bool txSendByteState = false;
static uint8_t txBuffer[256];
static volatile uint8_t txBufferHead, txBufferTail = 0;

static const int RS232_RTS = 3;
static SerialMouse* classPtr = NULL;


SerialMouse* SerialMouse::instance(mouseType_t mouseType) {
  if (classPtr == NULL) {
    classPtr = new SerialMouse(mouseType);
  }
  Serial.println(bitDelay);
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
  pktTime = twoButtons ? pktTime_2btn : pktTime_3btn;
  doSerialInit = true;
  serialInitDone = false;

  Serial.println("Listening on RTS");
  attachInterrupt(digitalPinToInterrupt(RS232_RTS), RTSInterruptStatic, FALLING);
}

void SerialMouse::sendByte(uint8_t data) {
  txBuffer[txBufferHead++] = data;
}

void SerialMouse::sendBytes(const uint8_t* data, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    txBuffer[txBufferHead + i] = data[i];
  }
  txBufferHead += len;
}

bool SerialMouse::update(const Ps2Report_t& data) {
  report.buttons = data.buttons;
  report.x += data.x;
  report.y += data.y;
  report.wheel += data.wheel;
  return (micros() - report.lastTime) > pktTime;
}

void SerialMouse::send() {

  report.lastTime = micros();

  if (report.x == 0 && report.y == 0 && report.wheel == 0 && report.buttons == report.lastButtons) {
    return;
  }

  uint8_t dataBytes[4] = {0, 0, 0, 0};
  uint8_t len = 3;
  auto dx = constrain(report.x, -127, 127);
  auto dy = constrain(-report.y, -127, 127);
  byte lb = (report.buttons & MB_LEFT) ? 0x20 : 0;
  byte rb = (report.buttons & MB_RIGHT) ? 0x10 : 0;
  dataBytes[0] = 0x40 | lb | rb | ((dy >> 4) & 0xC) | ((dx >> 6) & 0x3);
  dataBytes[1] = dx & 0x3F;
  dataBytes[2] = dy & 0x3F;

  if (!twoButtons) {
    byte mb;
    if (wheelMouse) {
      // according to: https://sourceforge.net/p/cutemouse/trunk/ci/master/tree/cutemouse/PROTOCOL.TXT
      // for a wheel mouse, the middle button should be reported in bit 0x10
      mb = (report.buttons & MB_MIDDLE) ? 0x10 : 0;
      mb |= (report.wheel & 0x0F);
    } else {
      // for a 3-button mouse, the middle button should be reported in bit 0x20
      mb = (report.buttons & MB_MIDDLE) ? 0x20 : 0;
    }
    dataBytes[3] = mb;
    len = 4;
  }
  sendBytes(dataBytes, len);
  report.lastButtons = report.buttons;
  report.x = report.y = report.wheel = 0;
}


ISR(TIMER1_COMPA_vect)
{
  static uint8_t txDataByte = 0;
  static uint8_t txDataBits = 0;
#ifdef USE_EXTRA_STOP_BIT
  static bool extraStopBit = true;
#endif

  if (!txSendByteState) {
    // if there is data to transmit
    if (txBufferTail != txBufferHead) {
      // switch state to byte transmission
      txSendByteState = true;
      // get the byte and set the byte width (7 for our case)
      txDataByte = txBuffer[txBufferTail++];
      txDataBits = UART_TX_DATA_BITS;
#ifdef USE_EXTRA_STOP_BIT
      extraStopBit = true;
#endif
      // Transmit the Start bit
      RS_SETTXLOW();
    }
  } else {
    // start bit is done, count down the data bits
    if (txDataBits) {
      txDataBits--;
      uint8_t bit = txDataByte & 0x01;
      txDataByte >>= 1;
      // Transmit the data bit
      RS_SETTX(bit);
    } else {
#ifdef USE_EXTRA_STOP_BIT
      // check for extra stop bit
      txSendByteState = extraStopBit;
      extraStopBit = false;
#else
      txSendByteState = false;
#endif
      // Transmit the Stop bit(s)
      RS_SETTXHIGH();
    }
  }
}


void SerialMouse::init() {
  TIMSK1 &= ~(1 << OCIE1A); // Disable timer compare interrupt
  Serial.println("Starting serial port");
  RS_SETTXHIGH();

  memset(&report, 0, sizeof(report));

  memset(txBuffer, 0, sizeof(txBuffer));
  txBufferHead = txBufferTail = 0;
  txSendByteState = false;

  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1  = 0; // initialize counter value to 0
  OCR1A = COMPA_1200_BAUD_BIT_VALUE; // 1200.0300007500186 BAUD
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
