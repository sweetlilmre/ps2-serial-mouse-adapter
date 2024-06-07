#pragma once

#include <Arduino.h>

class Ps2Mouse {
public:

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

  bool reset(bool streaming);

  bool setScaling(bool flag) const;
  bool setResolution(byte resolution) const;
  bool setSampleRate(byte sampleRate) const;

  bool getSettings(Settings& settings) const;

  bool enableStreaming();
  bool disableStreaming();
  bool readData(Data& data) const;

private:
  struct Impl;

  bool m_stream;
  bool m_wheelMouse;
};
