#include "Ps2Mouse.h"

namespace {

struct Status {

  byte rightButton   : 1;
  byte middleButton  : 1;
  byte leftButton    : 1;
  byte na2           : 1;
  byte scaling       : 1;
  byte dataReporting : 1;
  byte remoteMode    : 1;
  byte na1           : 1;

  byte resolution;
  byte sampleRate;
};

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
};

enum class Command {
  disableScaling        = 0xE6,
  enableScaling         = 0xE7,
  setResolution         = 0xE8,
  statusRequest         = 0xE9,
  setStreamMode         = 0xEA,
  readData              = 0xEB,
  resetWrapMode         = 0xEC, // Not implemented
  setWrapMode           = 0xEE, // Not implemented
  reset                 = 0xFF,
  setRemoteMode         = 0xF0,
  getDeviceId           = 0xF2, // Not implemented
  setSampleRate         = 0xF3,
  enableDataReporting   = 0xF4,
  disableDataReporting  = 0xF5,
  setDefaults           = 0xF6, // Not implemented
};

enum class Response {
  isMouse        = 0x00,
  selfTestPassed = 0xAA,
  ack            = 0xFA,
  error          = 0xFC,
  resend         = 0xFE,
};

} // namespace

struct Ps2Mouse::Impl {
  const Ps2Mouse& m_ref;

  void sendBit(int value) const {
    while (digitalRead(m_ref.m_clockPin) != LOW) {}
    digitalWrite(m_ref.m_dataPin, value);
    while (digitalRead(m_ref.m_clockPin) != HIGH) {}
  }

  int recvBit() const {
    while (digitalRead(m_ref.m_clockPin) != LOW) {}
    auto result = digitalRead(m_ref.m_dataPin);
    while (digitalRead(m_ref.m_clockPin) != HIGH) {}
    return result;
  }

  bool sendByte(byte value) const {

    // Inhibit communication
    pinMode(m_ref.m_clockPin, OUTPUT);
    digitalWrite(m_ref.m_clockPin, LOW);
    delayMicroseconds(100);

    // Set start bit and release the clock
    pinMode(m_ref.m_dataPin, OUTPUT);
    digitalWrite(m_ref.m_dataPin, LOW);
    pinMode(m_ref.m_clockPin, INPUT_PULLUP);

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
    pinMode(m_ref.m_dataPin, INPUT);
    return recvBit() == 0;
  }

  bool recvByte(byte& value) const {

    // Enter receive mode
    pinMode(m_ref.m_clockPin, INPUT);
    pinMode(m_ref.m_dataPin, INPUT);

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

    // Receive and check parity bit
    recvBit(); // TODO check parity

    // Receive stop bit
    recvBit();

    return true;
  }

  template <typename T>
  bool sendData(const T& data) const {
    auto ptr = reinterpret_cast<const byte*>(&data);
    for (auto i = 0; i < sizeof(data); i++) {
      if (!sendByte(ptr[i])) {
        return false;
      }
    }
    return true;
  }

  template <typename T>
  bool recvData(T& data) const {
    auto ptr = reinterpret_cast<byte*>(&data);
    for (auto i = 0; i < sizeof(data); i++) {
      if (!recvByte(ptr[i])) {
        return false;
      }
    }
    return true;
  }

  bool sendByteWithAck(byte value) const {
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

  bool sendCommand(Command command) const {
    return sendByteWithAck(static_cast<byte>(command));
  }

  bool sendSetting(Command command, byte setting) const {
    if (sendCommand(command)) {
      return sendByteWithAck(setting);
    }
    return false;
  }

  bool getStatus(Status& status) const {
    if (sendCommand(Command::statusRequest)) {
      return recvData(status);
    }
    return false;
  }

  bool setMode(Ps2Mouse::Mode mode) const {
    switch (mode) {
      case Ps2Mouse::Mode::remote:
        return sendCommand(Command::setRemoteMode);
      case Ps2Mouse::Mode::stream:
        return sendCommand(Command::setStreamMode);
    }
    return false;
  }

};

Ps2Mouse::Ps2Mouse(int clockPin, int dataPin, Mode mode)
  : m_clockPin(clockPin), m_dataPin(dataPin), m_mode(mode)
{}

bool Ps2Mouse::reset() const {
  Impl impl{*this};
  if (impl.sendCommand(Command::reset)) {
    byte reply;
    impl.recvByte(reply);
    if (reply == static_cast<byte>(Response::selfTestPassed)) {
      impl.recvByte(reply);
      if (reply == static_cast<byte>(Response::isMouse)) {
        return impl.setMode(m_mode);
      }
    }
  }
  return false;
}

bool Ps2Mouse::setDataReporting(bool flag) const {
  Command command = flag ? Command::enableDataReporting : Command::disableDataReporting;
  return Impl{*this}.sendCommand(command);
}

bool Ps2Mouse::getDataReporting(bool& flag) const {
  Status status;
  if (Impl{*this}.getStatus(status)) {
    flag = status.dataReporting;
    return true;
  }
  return false;
}

bool Ps2Mouse::setScaling(bool flag) const {
  Command command = flag ? Command::enableScaling : Command::disableScaling;
  return Impl{*this}.sendCommand(command);
}

bool Ps2Mouse::getScaling(bool& flag) const {
  Status status;
  if (Impl{*this}.getStatus(status)) {
    flag = status.scaling;
    return true;
  }
  return false;
}

bool Ps2Mouse::setResolution(byte resolution) const {
  return Impl{*this}.sendSetting(Command::setResolution, resolution);
}

bool Ps2Mouse::getResolution(byte& resolution) const {
  Status status;
  if (Impl{*this}.getStatus(status)) {
    resolution = status.resolution;
    return true;
  }
  return false;
}

bool Ps2Mouse::setSampleRate(byte sampleRate) const {
  return Impl{*this}.sendSetting(Command::setSampleRate, sampleRate);
}

bool Ps2Mouse::getSampleRate(byte& sampleRate) const {
  Status status;
  if (Impl{*this}.getStatus(status)) {
    sampleRate = status.sampleRate;
    return true;
  }
  return false;
}

bool Ps2Mouse::hasData() const {
  return (m_mode == Mode::remote) || (digitalRead(m_clockPin) == LOW);
}

bool Ps2Mouse::readData(Data& data) const {

  Impl impl{*this};

  if (m_mode == Mode::remote && !impl.sendCommand(Command::readData)) {
    return false;
  }

  Packet packet;
  if (!impl.recvData(packet)) {
    return false;
  }

  data.leftButton = packet.leftButton;
  data.middleButton = packet.middleButton;
  data.rightButton = packet.rightButton;
  data.xMovement = packet.xSign ? (0xFF00 | packet.xMovement) : packet.xMovement;
  data.yMovement = packet.ySign ? (0xFF00 | packet.yMovement) : packet.yMovement;
  return true;
}
