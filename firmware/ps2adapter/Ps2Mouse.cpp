#include "ProMini.h"
#include "Ps2Mouse.h"

namespace {

struct Packet {

  byte leftButton    : 1;
  byte rightButton   : 1;
  byte middleButton  : 1;
  byte na            : 1;
  byte xSign         : 1;
  byte ySign         : 1;
  byte xOverflow     : 1;
  byte yOverflow     : 1;

  byte xMovement;
  byte yMovement;
  byte wheelData;
};

enum class Response {

  isMouse        = 0x00,
  isWheelMouse   = 0x03,
  selfTestPassed = 0xAA,
  ack            = 0xFA,
  error          = 0xFC,
  resend         = 0xFE,
};

} // namespace

static Ps2Mouse* classPtr = NULL;

Ps2Mouse* Ps2Mouse::instance() {

  if (classPtr == NULL) {
    classPtr = new Ps2Mouse();
  }
  return classPtr;
}

Ps2Mouse::Ps2Mouse()
{
 
  m_type = MouseType::threeButton;
  m_mouseBits = 0;
  m_bitCount = 0;
  m_parityBit = 0;
  m_bufferTail = 0;
  m_bufferHead = 0;
  m_bufferCount = 0;
  memset(m_buffer, 0, 256);
}

bool Ps2Mouse::reset() {
  
  byte reply;

  if (!sendCommand(Command::reset)) {
      return false;
  }

  if (!getByte(reply) || reply != byte(Response::selfTestPassed)) {
      return false;
  }

  if (!getByte(reply) || reply != byte(Response::isMouse)) {
      return false;
  }

  // Determine if this is a wheel mouse
  if (!sendCommand(Command::setSampleRate, 200) ||
      !sendCommand(Command::setSampleRate, 100) ||
      !sendCommand(Command::setSampleRate, 80) ||
      !sendCommand(Command::getDeviceId) ||
      !getByte(reply)) {
      return false;
  }

  if (reply == byte(Response::isWheelMouse)) {
    m_type = MouseType::wheelMouse;
  }

  // switch on data reporting
  return setReporting(true);
}

bool Ps2Mouse::setReporting(bool enable) {
  
  return sendCommand(enable ? Command::enableDataReporting : Command::disableDataReporting);
}

bool Ps2Mouse::setStreamMode() {
  
  return (sendCommand(Command::setStreamMode));
}

bool Ps2Mouse::setRemoteMode() {
  
  return (sendCommand(Command::setRemoteMode));
}

bool Ps2Mouse::setScaling(bool flag) {
  
  setReporting(false);
  bool res =  sendCommand(flag ? Command::enableScaling : Command::disableScaling);
  setReporting(true);
  return res;
}

bool Ps2Mouse::setResolution(byte resolution) {
  
  setReporting(false);
  bool res = sendCommand(Command::setResolution, resolution);
  setReporting(true);
  return res;
}

bool Ps2Mouse::setSampleRate(byte sampleRate) {
  
  setReporting(false);
  bool res = sendCommand(Command::setSampleRate, sampleRate);
  setReporting(true);
  return res;
}

bool Ps2Mouse::getSettings(Settings& settings) {
  
  Status status;
  setReporting(false);
  bool res = getStatus(status);
  setReporting(true);
  if (res) {
    settings.rightBtn = status.rightButton;
    settings.middleBtn = status.middleButton;
    settings.leftBtn = status.leftButton;
    settings.remoteMode = status.remoteMode;
    settings.enable = status.dataReporting;
    settings.scaling = status.scaling;
    settings.resolution = status.resolution;
    settings.sampleRate = status.sampleRate;
    return true;
  }
  return false;
}

Ps2Mouse::MouseType Ps2Mouse::getType() const {
  return m_type;
}

void Ps2Mouse::clockInterruptStatic() {

  if (classPtr != NULL) {
    classPtr->interruptHandler();
  }
}

void Ps2Mouse::startInterrupt() {

  // Enter receive mode
  PS2_DIRCLOCKIN();
  PS2_DIRDATAIN();
  m_mouseBits = 0;
  m_bitCount = 0;
  m_bufferHead = m_bufferTail = m_bufferCount = 0;
  memset(m_buffer, 0, 256);
  attachInterrupt(0, clockInterruptStatic, FALLING);
}

void Ps2Mouse::stopInterrupt() {
  detachInterrupt(0);
  m_mouseBits = 0;
  m_bitCount = 0;
  m_bufferHead = m_bufferTail = m_bufferCount = 0;
  memset(m_buffer, 0, 256);
}


bool Ps2Mouse::readData(Data& data) {

  size_t pktSize = sizeof(Packet);
  if (m_type != MouseType::wheelMouse) {
    pktSize--;
  }

  if (m_bufferCount < pktSize) {
    return false;
  }

  Packet packet = {0};
  if (m_type == MouseType::wheelMouse) {
    if (!getBytes((byte*)&packet, sizeof(packet))) {
      return false;
    }
  } else {
    if (!getBytes((byte*)&packet, sizeof(packet) - 1)) {
      return false;
    }
  }

  data.leftButton = packet.leftButton;
  data.middleButton = packet.middleButton;
  data.rightButton = packet.rightButton;
  data.xMovement = (packet.xSign ? -0x100 : 0) | packet.xMovement;
  data.yMovement = (packet.ySign ? -0x100 : 0) | packet.yMovement;
  data.wheelMovement = (m_type == MouseType::wheelMouse) ? packet.wheelData : 0;
  return true;
}


void Ps2Mouse::interruptHandler() {
  
  uint8_t bit = PS2_READDATA();

  if (m_bitCount == 0) {
    // start bit should ALWAYS be 0
    if (bit) {
      // LED_SETHIGH();
      // error on start bit
      return;
    }
    // start bit: bit 1
    m_parityBit = 1;
    m_mouseBits = 0;
    m_bitCount++;
  } else if ((m_bitCount > 0) && (m_bitCount < 9)) {
    // data bits: bits 2 - 9
    m_mouseBits >>= 1;
    m_mouseBits |= (bit << 7);
    m_parityBit ^= bit;
    m_bitCount++;
  } else if (m_bitCount == 9) {
    // parity bit: bit 10
    // parity bit should match the calculated parity
    // so m_parityBit xor bit will be 0 if parity matches:
    // 1 ^ 1 = 0
    // 0 ^ 0 = 0
    // 1 ^ 0 = 1
    // 0 ^ 1 = 1
    m_parityBit ^= bit;
    m_bitCount++;
  } else {
    // stop bit: bit 11
    // if stop bit is 1 and parity 0, then add this byte to the buffer
    if (bit && !m_parityBit) {
      m_buffer[m_bufferHead] = m_mouseBits;
      m_bufferCount++;
      m_bufferHead++;
      m_bitCount++;
      //LED_SETLOW();
    } else {
      // LED_SETHIGH();
    }
    m_bitCount = 0;
  }
}

bool Ps2Mouse::getByte(uint8_t& data) {
  unsigned long count = 0;
  while (m_bufferHead == m_bufferTail) {
    if (count++ > 5000000) {
      return false;
    }
  }
  m_bufferCount--;
  data = m_buffer[m_bufferTail++];
  return true;
}

bool Ps2Mouse::getBytes(uint8_t* data, size_t size) {

  for (size_t i = 0u; i < size; i++) {
    if (!getByte(data[i])) {
      return false;
    }
  }
  return true;
}

void Ps2Mouse::sendBit(int value) const {
  
  while (PS2_READCLOCK() != LOW) {}
  PS2_SETDATA(value);
  while (PS2_READCLOCK() != HIGH) {}
}

bool Ps2Mouse::sendByte(byte value) {

  // Inhibit communication
  stopInterrupt();
  PS2_DIRCLOCKOUT();
  PS2_SETCLOCKLOW();
  delayMicroseconds(10);

  // Set start bit and release the clock
  PS2_DIRDATAOUT();
  PS2_SETDATALOW();
  PS2_DIRCLOCKIN_UP();

  // Send data bits
  byte parity = 1;
  for (auto i = 0; i < 8; i++) {
    byte nextBit = (value >> i) & 0x01;
    parity ^= nextBit;
    sendBit(nextBit);
  }

  // Send parity bit
  sendBit(parity);

  // Send stop bit
  sendBit(1);

  // Enter receive mode and wait for ACK bit
  PS2_DIRDATAIN();
  while (PS2_READCLOCK() != LOW) {}
  byte ack = PS2_READDATA();
  while (PS2_READCLOCK() != HIGH) {}

  startInterrupt();
  return ack == 0;
}

bool Ps2Mouse::sendByteWithAck(byte value) {
  while (true) {
    if (sendByte(value)) {
      byte response;
      if (getByte(response)) {
        if (response == static_cast<byte>(Response::resend)) {
          continue;
        }
        return response == static_cast<byte>(Response::ack);
      }
    }
    return false;
  }
}

bool Ps2Mouse::sendCommand(Command command) {
  return sendByteWithAck(static_cast<byte>(command));
}

bool Ps2Mouse::sendCommand(Command command, byte setting) {
  return sendCommand(command) && sendByteWithAck(setting);
}

bool Ps2Mouse::getStatus(Status& status) {
  return sendCommand(Command::statusRequest) && getBytes((byte*) &status, sizeof(Status));
}
