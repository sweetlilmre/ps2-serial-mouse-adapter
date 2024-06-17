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


Ps2Mouse::Ps2Mouse()
  : m_type(MouseType::threeButton)
{}

bool Ps2Mouse::reset() {
  
  byte reply;

  if (!sendCommand(Command::reset)) {
      return false;
  }

  if (!recvByte(reply) || reply != byte(Response::selfTestPassed)) {
      return false;
  }

  if (!recvByte(reply) || reply != byte(Response::isMouse)) {
      return false;
  }

  // Determine if this is a wheel mouse
  if (!sendCommand(Command::setSampleRate, 200) ||
      !sendCommand(Command::setSampleRate, 100) ||
      !sendCommand(Command::setSampleRate, 80) ||
      !sendCommand(Command::getDeviceId) ||
      !recvByte(reply)) {
      return false;
  }

  if (reply == byte(Response::isWheelMouse)) {
    m_type = MouseType::wheelMouse;
  }

  // switch on data reporting
  return setReporting(true);
}

bool Ps2Mouse::setReporting(bool enable) const {
  
  return sendCommand(enable ? Command::enableDataReporting : Command::disableDataReporting);
}

bool Ps2Mouse::setStreamMode() {
  
  return (sendCommand(Command::setStreamMode));
}

bool Ps2Mouse::setRemoteMode() {
  
  return (sendCommand(Command::setRemoteMode));
}

bool Ps2Mouse::setScaling(bool flag) const {
  
  setReporting(false);
  bool res =  sendCommand(flag ? Command::enableScaling : Command::disableScaling);
  setReporting(true);
  return res;
}

bool Ps2Mouse::setResolution(byte resolution) const {
  
  setReporting(false);
  bool res = sendCommand(Command::setResolution, resolution);
  setReporting(true);
  return res;
}

bool Ps2Mouse::setSampleRate(byte sampleRate) const {
  
  setReporting(false);
  bool res = sendCommand(Command::setSampleRate, sampleRate);
  setReporting(true);
  return res;
}

bool Ps2Mouse::getSettings(Settings& settings) const {
  
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

bool Ps2Mouse::readData(Data& data) const {

  if (PS2_READCLOCK != LOW) {
     return false;
  }

  Packet packet = {0};
  if (m_type == MouseType::wheelMouse) {
    if (!recvData((byte*)&packet, sizeof(packet))) {
      return false;
    }
  } else {
    if (!recvData((byte*)&packet, sizeof(packet) - 1)) {
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

void Ps2Mouse::sendBit(int value) const {
  
  while (PS2_READCLOCK != LOW) {}
  PS2_SETDATA(value);
  while (PS2_READCLOCK != HIGH) {}
}

bool Ps2Mouse::sendByte(byte value) const {

  // Inhibit communication
  PS2_DIRCLOCKOUT;
  PS2_SETCLOCKLOW;
  delayMicroseconds(10);

  // Set start bit and release the clock
  PS2_DIRDATAOUT;
  PS2_SETDATALOW;
  PS2_DIRCLOCKIN_UP;

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
  PS2_DIRDATAIN;
  return recvBit() == 0;
}

int Ps2Mouse::recvBit() const {

  while (PS2_READCLOCK != LOW) {}
  auto result = PS2_READDATA;
  while (PS2_READCLOCK != HIGH) {}
  return result;
}

bool Ps2Mouse::recvByte(byte& value) const {

  // Enter receive mode
  PS2_DIRCLOCKIN;
  PS2_DIRDATAIN;

  // Receive start bit
  if (recvBit() != 0) {
    return false;
  }

  // Receive data bits
  value = 0;
  byte parity = 1;
  for (int i = 0; i < 8; i++) {
    byte nextBit = recvBit();
    value |= nextBit << i;
    parity ^= nextBit;
  }

  // Receive parity bit
  byte actualParity = recvBit();
  if (parity != actualParity) {
    Serial.println("P!");
  }

  // Receive stop bit
  recvBit();

  return parity == actualParity;
}

bool Ps2Mouse::recvData(byte* data, size_t size) const {
  for (size_t i = 0u; i < size; i++) {
    if (!recvByte(data[i])) {
      return false;
    }
  }
  return true;
}

bool Ps2Mouse::sendByteWithAck(byte value) const {
  while (true) {
    if (sendByte(value)) {
      byte response;
      if (recvByte(response)) {
        if (response == static_cast<byte>(Response::resend)) {
          continue;
        }
        return response == static_cast<byte>(Response::ack);
      }
    }
    return false;
  }
}

bool Ps2Mouse::sendCommand(Command command) const {
  return sendByteWithAck(static_cast<byte>(command));
}

bool Ps2Mouse::sendCommand(Command command, byte setting) const {
  return sendCommand(command) && sendByteWithAck(setting);
}

bool Ps2Mouse::getStatus(Status& status) const {
  return sendCommand(Command::statusRequest) && recvData((byte*) &status, sizeof(Status));
}
