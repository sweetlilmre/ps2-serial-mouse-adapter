#pragma once

#include <Arduino.h>

class Ps2Mouse {
public:

  enum class MouseType {

    threeButton,
    wheelMouse,
  };

  struct Data {

    bool leftButton;
    bool middleButton;
    bool rightButton;
    int  xMovement;
    int  yMovement;
    int  wheelMovement;
  };

  struct Settings {
    
    bool rightBtn;
    bool middleBtn;
    bool leftBtn;
    bool scaling;
    bool enable;
    bool remoteMode;
    byte resolution;
    byte sampleRate;
  };

  Ps2Mouse();

  bool reset();
  bool setReporting(bool enable) const;

  bool setStreamMode();
  bool setRemoteMode();

  bool setScaling(bool flag) const;
  bool setResolution(byte resolution) const;
  bool setSampleRate(byte sampleRate) const;

  bool getSettings(Settings& settings) const;
  Ps2Mouse::MouseType getType() const;

  bool readData(Data& data) const;
  void startInterrupt();
  void stopInterrupt();
  bool getByte(uint8_t* data);
  bool getBytes(uint8_t* data, int length);
  bool readData2(Data& data);
  uint8_t getBufferCount() { return m_bufferCount; };
  uint8_t getBufferHead() { return m_bufferHead; };
  uint8_t getBufferTail() { return m_bufferTail; };
private:
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

  void sendBit(int value) const;
  int recvBit() const;
  bool sendByte(byte value) const;
  bool recvByte(byte& value) const;
  bool recvData(byte* data, size_t size) const;
  bool sendByteWithAck(byte value) const;
  bool sendCommand(Command command) const;
  bool sendCommand(Command command, byte setting) const;
  bool getStatus(Status& status) const;
  
  static void clockInterruptStatic();
  void interruptHandler();

  MouseType m_type;
  uint8_t m_mouseBits;
  uint8_t m_bitCount;
  uint8_t m_parityBit;
  volatile uint8_t m_bufferTail, m_bufferHead, m_bufferCount;
  uint8_t m_buffer[256];
};
