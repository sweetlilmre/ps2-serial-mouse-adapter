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
static uint8_t mouseBits;
static uint8_t bitCount;
static uint8_t parityBit;
static volatile uint8_t bufferTail, bufferHead, bufferCount;
static uint8_t buffer[256];

static void clockInterrupt() {
  uint8_t bit = PS2_READDATA();

  if (bitCount == 0) {
    // start bit should ALWAYS be 0
    if (bit) {
      // LED_SETHIGH();
      // error on start bit
      return;
    }
    // start bit: bit 1
    parityBit = 1;
    mouseBits = 0;
    bitCount++;
  } else if (bitCount < 9) {
    // data bits: bits 2 - 9
    mouseBits >>= 1;
    mouseBits |= (bit << 7);
    parityBit ^= bit;
    bitCount++;
  } else if (bitCount == 9) {
    // parity bit: bit 10
    // parity bit should match the calculated parity
    // so parityBit xor bit will be 0 if parity matches:
    // 1 ^ 1 = 0
    // 0 ^ 0 = 0
    // 1 ^ 0 = 1
    // 0 ^ 1 = 1
    parityBit ^= bit;
    bitCount++;
  } else {
    // stop bit: bit 11
    // if stop bit is 1 and parity 0, then add this byte to the buffer
    if (bit && !parityBit) {
      buffer[bufferHead] = mouseBits;
      bufferCount++;
      bufferHead++;
      //LED_SETLOW();
    } else {
      // LED_SETHIGH();
    }
    bitCount = 0;
  }
}


Ps2Mouse* Ps2Mouse::instance() {

  if (classPtr == NULL) {
    classPtr = new Ps2Mouse();
  }
  return classPtr;
}

Ps2Mouse::Ps2Mouse()
{
  // PS/2 Data input must be initialized shortly after power on,
  // or the mouse will not initialize
  PS2_DIRDATAIN_UP();
  m_type = mouseType_t::threeButton;
  mouseBits = 0;
  bitCount = 0;
  parityBit = 0;
  bufferTail = 0;
  bufferHead = 0;
  bufferCount = 0;
  memset(buffer, 0, 256);
}

uint8_t Ps2Mouse::getBufferCount() const {
   return bufferCount; 
};

bool Ps2Mouse::init() {

  Serial.print("Reseting PS/2 mouse... ");
  
  if (reset()) {
    Serial.println("OK");
  } else {
    Serial.println("Failed!");
    return false;
  }

  if (setSampleRate(20)) {
    Serial.println("Sample rate set to 20");
  } else {
    Serial.println("Failed to set sample rate");
    return false;
  }

  Ps2Settings_t settings;
  if (getSettings(settings)) {
    Serial.print("scaling = ");
    Serial.println(settings.scaling);
    Serial.print("resolution = ");
    Serial.println(settings.resolution);
    Serial.print("samplingRate = ");
    Serial.println(settings.sampleRate);
  } else {
    Serial.println("Failed to get settings");
    return false;
  }
  return true;
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
    m_type = mouseType_t::wheelMouse;
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

bool Ps2Mouse::getSettings(Ps2Settings_t& settings) {
  
  Status status;
  setReporting(false);
  bool res = getStatus(status);
  setReporting(true);
  if (res) {
    settings.buttons = (status.middleButton << MB_MIDDLE_BITPOS) | (status.rightButton << MB_RIGHT_BITPOS) | (status.leftButton << MB_LEFT_BITPOS);
    settings.remoteMode = status.remoteMode;
    settings.enable = status.dataReporting;
    settings.scaling = status.scaling;
    settings.resolution = status.resolution;
    settings.sampleRate = status.sampleRate;
    return true;
  }
  return false;
}

mouseType_t Ps2Mouse::getType() const {
  return m_type;
}


void Ps2Mouse::startInterrupt() {

  // Enter receive mode
  PS2_DIRCLOCKIN();
  PS2_DIRDATAIN();
  mouseBits = 0;
  bitCount = 0;
  bufferHead = bufferTail = bufferCount = 0;
  memset(buffer, 0, 256);
  attachInterrupt(0, clockInterrupt, FALLING);
}

void Ps2Mouse::stopInterrupt() {
  detachInterrupt(0);
  mouseBits = 0;
  bitCount = 0;
  bufferHead = bufferTail = bufferCount = 0;
  memset(buffer, 0, 256);
}


bool Ps2Mouse::readData(Ps2Report_t& data) {

  size_t pktSize = sizeof(Packet);
  if (m_type != mouseType_t::wheelMouse) {
    pktSize--;
  }

  if (bufferCount < pktSize) {
    return false;
  }

  Packet packet = {0};
  if (m_type == mouseType_t::wheelMouse) {
    if (!getBytes(reinterpret_cast<uint8_t*>(&packet), sizeof(packet))) {
      return false;
    }
  } else {
    if (!getBytes(reinterpret_cast<uint8_t*>(&packet), sizeof(packet) - 1)) {
      return false;
    }
  }

  data.buttons = (packet.middleButton ? MB_MIDDLE : 0) | (packet.rightButton ? MB_RIGHT : 0) | (packet.leftButton ? MB_LEFT : 0);
  data.x = (packet.xSign ? -0x100 : 0) | packet.xMovement;
  data.y = (packet.ySign ? -0x100 : 0) | packet.yMovement;
  data.wheel = (m_type == mouseType_t::wheelMouse) ? packet.wheelData : 0;
  return true;
}


bool Ps2Mouse::getByte(uint8_t& data) {
  unsigned long count = 0;
  while (bufferHead == bufferTail) {
    if (count++ > 5000000) {
      return false;
    }
  }
  bufferCount--;
  data = buffer[bufferTail++];
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
  return sendCommand(Command::statusRequest) && getBytes(reinterpret_cast<uint8_t*>(&status), sizeof(Status));
}
