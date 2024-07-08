#pragma once

#include <Arduino.h>
#include "MouseData.h"

class Ps2Mouse {

public:
  static Ps2Mouse* instance();

  bool init();
  bool reset();
  bool setReporting(bool enable);

  bool setStreamMode();
  bool setRemoteMode();

  bool setScaling(bool flag);
  bool setResolution(byte resolution);
  bool setSampleRate(byte sampleRate);

  bool getSettings(Ps2Settings_t& settings);
  mouseType_t getType() const;

  bool readData(Ps2Report_t& data);
  uint8_t getBufferCount() const { return m_bufferCount; };

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

  Ps2Mouse();

  bool getByte(uint8_t& data);
  bool getBytes(uint8_t* data, size_t size);

  void sendBit(int value) const;
  bool sendByte(byte value);
  bool sendByteWithAck(byte value);
  bool sendCommand(Command command);
  bool sendCommand(Command command, byte setting);
  bool getStatus(Status& status);
  
  void startInterrupt();
  void stopInterrupt();
  static void clockInterruptStatic();
  void interruptHandler();

  mouseType_t m_type;
  uint8_t m_mouseBits;
  uint8_t m_bitCount;
  uint8_t m_parityBit;
  volatile uint8_t m_bufferTail, m_bufferHead, m_bufferCount;
  uint8_t m_buffer[256];
};
