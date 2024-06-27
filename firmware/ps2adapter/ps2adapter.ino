#include "ProMini.h"
#include "Ps2Mouse.h"
#include "LEDMessenger.h"

static const int RS232_RTS = 3;

static Ps2Mouse* pMouse;
static bool twoButtons = false;
static bool wheelMouse = false;
static volatile bool doSerialInit = true;
static volatile bool serialInitDone = false;

static void sendSerialBit(int data) {
  // Delay between the signals to match 1200 baud
  static const auto usDelay = 1000000 / 1200;
  RS_SETTX(data);
  delayMicroseconds(usDelay);
}

static void sendSerialByte(byte data) {

  // Start bit
  sendSerialBit(0);

  // Data bits
  for (int i = 0; i < 7; i++) {
    sendSerialBit((data >> i) & 0x01);
  }

  // Stop bit
  sendSerialBit(1);

  // 7+1 bits is normal mouse protocol, but some serial controllers
  // expect 8+1 bits format. We send additional stop bit to stay
  // compatible to that kind of controllers.
  sendSerialBit(1);
}

static void sendToSerial(const Ps2Mouse::Data& data) {

  auto dx = constrain(data.xMovement, -127, 127);
  auto dy = constrain(-data.yMovement, -127, 127);
  byte lb = data.leftButton ? 0x20 : 0;
  byte rb = data.rightButton ? 0x10 : 0;
  sendSerialByte(0x40 | lb | rb | ((dy >> 4) & 0xC) | ((dx >> 6) & 0x3));
  sendSerialByte(dx & 0x3F);
  sendSerialByte(dy & 0x3F);
  if (!twoButtons) {
    byte mb;
    if (wheelMouse) {
      // according to: https://sourceforge.net/p/cutemouse/trunk/ci/master/tree/cutemouse/PROTOCOL.TXT
      // for a wheel mouse, the middle button should be reported in bit 0x10
      mb = data.middleButton ? 0x10 : 0;
      mb |= (data.wheelMovement & 0x0F);
    } else {
      // for a 3-button mouse, the middle button should be reported in bit 0x20
      mb = data.middleButton ? 0x20 : 0;
    }
    sendSerialByte(mb);
  }
}

static void initSerialPort() {

  // disable the PS/2 mouse data reporting while the serial port is being initialized
  pMouse->setReporting(false);

  Serial.println("Starting serial port");
  RS_SETTXHIGH();
  delayMicroseconds(10000);
  sendSerialByte('M');
  if(!twoButtons) {
    // if wheelMouse mode has been set by jumper, confirm that the mouse has the capability
    if(wheelMouse && (pMouse->getType() == Ps2Mouse::MouseType::wheelMouse)) {
      // set wheelMouse mode
      sendSerialByte('Z');
      Serial.println("Init wheel mode");
    } else {
      // set 3-button (Logitech) mode
      sendSerialByte('3');
      Serial.println("Init 3-button mode");
    }
  } else {
    Serial.println("Init 2-button mode");
  }
  delayMicroseconds(10000);
  // Renable PS/2 mouse data reporting
  pMouse->setReporting(true);
  // Signal the main loop that the serial port is ready
  serialInitDone = true;
}

static void initPs2Port() {

  Serial.print("Reseting PS/2 mouse... ");
  
  if (pMouse->reset()) {
    Serial.println("OK");
  } else {
    Serial.println("Failed!");
  }

  if (pMouse->setSampleRate(20)) {
    Serial.println("Sample rate set to 20");
  } else {
    Serial.println("Failed to set sample rate");
  }

  Ps2Mouse::Settings settings;
  if (pMouse->getSettings(settings)) {
    Serial.print("scaling = ");
    Serial.println(settings.scaling);
    Serial.print("resolution = ");
    Serial.println(settings.resolution);
    Serial.print("samplingRate = ");
    Serial.println(settings.sampleRate);
  } else {
    Serial.println("Failed to get settings");
  }
}

void resetSerialSignal() {

  // signal the main loop to ignore the PS/2 mouse data until the serial port is ready
  serialInitDone = false;
  // signal the main loop to initialize the serial port
  doSerialInit = true;
}

LEDMessenger blinker;

void setup() {
  // PS/2 Data input must be initialized shortly after power on,
  // or the mouse will not initialize
  PS2_DIRDATAIN_UP();
  RS_DIRTXOUT();
  JP12_DIRIN_UP();
  JP34_DIRIN_UP();
  LED_DIROUT();
  LED_SETHIGH();

  Serial.begin(115200);
  blinker.start();

  twoButtons = (JP12_READ() == LOW);
  wheelMouse = (JP34_READ() == LOW);
  pMouse = Ps2Mouse::instance();

  // PS/2 initialization can take > 400ms for reset to complete.
  // Init the mouse here, determine its characteristics and then
  // wait for the RTS signal to initialize the serial port comms
  initPs2Port();
  Serial.println("Listening on RTS");
  attachInterrupt(digitalPinToInterrupt(RS232_RTS), resetSerialSignal, FALLING);

  Serial.println("Setup done!");
  LED_SETLOW();
}

enum class SettingState {
  ProcessMouse,
  SettingEnterDetected,
  SettingStateConfirmed,
};


static SettingState settingState = SettingState::ProcessMouse;
static unsigned long lastMillis = 0;
static bool swapButtons = false;

void processStateMachine() {

  Ps2Mouse::Data data;
  bool validData = pMouse->readData(data);

  switch (settingState) {
    case SettingState::ProcessMouse:
      if (!validData) {
        return;
      }
      if (data.leftButton && data.rightButton && data.middleButton) {
        settingState = SettingState::SettingEnterDetected;
        lastMillis = millis();
        blinker.push(500, -1, 500);
      } else {
        if (swapButtons) {
          bool tmp = data.leftButton;
          data.leftButton = data.rightButton;
          data.rightButton = tmp;
        }
        Serial.println("L: " + String(data.leftButton) + " M: " + String(data.middleButton) + " R: " + String(data.rightButton) + " X: " + String(data.xMovement) + " Y: " + String(data.yMovement) + " W: " + String(data.wheelMovement));
        sendToSerial(data);
      }
      break;
    case SettingState::SettingEnterDetected:
      if (validData && !(data.leftButton && data.rightButton && data.middleButton)) {
        settingState = SettingState::ProcessMouse;
        blinker.push(0, 0, 200);
        return;
      }
      if (millis() - lastMillis > 3000) {
        settingState = SettingState::SettingStateConfirmed;
        lastMillis = millis();
        blinker.push(300, -1, 500);
      }
      break;
    case SettingState::SettingStateConfirmed:
      if (!validData) {
        return;
      }
      
      if (millis() - lastMillis > 1000) {
        // button swap mode selected
        if (data.rightButton && !data.leftButton && !data.middleButton) {
          swapButtons = !swapButtons;
          Serial.println("Button Swap: " + String(swapButtons));
          settingState = SettingState::ProcessMouse;
          blinker.push(200, 3, 1000);
        }
      }
      break;
  }
}

void loop() {
  if (doSerialInit) {
    initSerialPort();
    doSerialInit = false;
  }

  if (serialInitDone) {
    processStateMachine();
  }
}
