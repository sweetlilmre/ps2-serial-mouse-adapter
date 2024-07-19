#include "ProMini.h"
#include "Ps2Mouse.h"
#include "SerialMouse.h"
#include "LEDMessenger.h"


static Ps2Mouse* pPs2Mouse;
static SerialMouse* pSerialMouse;
LEDMessenger blinker;



void setup() {
  LED_DIROUT();
  LED_SETHIGH();
  Serial.begin(115200);
  blinker.start();

  pPs2Mouse = Ps2Mouse::instance();
  // PS/2 initialization can take > 400ms for reset to complete.
  // Init the mouse here, determine its characteristics.
  // Serial initialization will happen in the main loop once on startup and then triggered by the RTS signal.
  if (!pPs2Mouse->init()) {
    Serial.println("PS/2 mouse initialization failed!");
    blinker.push(50, -1, 0);
  }

  pSerialMouse = SerialMouse::instance(pPs2Mouse->getType());

  

  Serial.println("Setup done!");
  LED_SETLOW();
}

enum class SettingState {
  ProcessMouse,
  SettingEnterDetected,
  SettingEnterConfirmed,
  SettingEnterReady,
};


static SettingState settingState = SettingState::ProcessMouse;
static unsigned long lastMillis = 0;
static bool swapButtons = false;
static unsigned long count = 0;

void processStateMachine() {

  Ps2Report_t data;
  bool validData = pPs2Mouse->readData(data);

  switch (settingState) {
    case SettingState::ProcessMouse:
      if (!validData) {
        return;
      }
      if (data.buttons == MB_ALL) {
        settingState = SettingState::SettingEnterDetected;
        lastMillis = millis();
        blinker.push(500, -1, 0);
      } else {
        if (swapButtons) {
          uint8_t temp = (data.buttons & MB_MIDDLE) | MB_L_TO_R(data.buttons) | MB_R_TO_L(data.buttons);
          data.buttons = temp;
        }
        /*
        Serial.println("C:" + String(count++) +
                       "L: " + String(MB_LEFT_STATE(data.buttons)) + 
                       " M: " + String(MB_MIDDLE_STATE(data.buttons)) + 
                       " R: " + String(MB_RIGHT_STATE(data.buttons)) + 
                       " X: " + String(data.x) + 
                       " Y: " + String(data.y) + 
                       " W: " + String(data.wheel));
                       */
        Serial.println(count++);
        pSerialMouse->send(data);
      }
      break;

    case SettingState::SettingEnterDetected:
      if (validData && (data.buttons != MB_ALL)) {
        settingState = SettingState::ProcessMouse;
        blinker.push(0, 0, 200);
        return;
      }
      if (millis() - lastMillis > 3000) {
        settingState = SettingState::SettingEnterConfirmed;
        lastMillis = millis();
        blinker.push(300, -1, 500);
      }
      break;

    case SettingState::SettingEnterConfirmed:
      if (!validData) {
        return;
      }
      if (millis() - lastMillis > 1000 && data.buttons == 0) {
        settingState = SettingState::SettingEnterReady;
      }
      break;

    case SettingState::SettingEnterReady:
      if (!validData) {
        return;
      }

      // button swap mode selected
      if (data.buttons == MB_RIGHT) {
        swapButtons = !swapButtons;
        Serial.println("Button Swap: " + String(swapButtons));
        settingState = SettingState::ProcessMouse;
        blinker.push(200, swapButtons ? 3 : 2, 1000);
      } else if (data.buttons == MB_LEFT) {
        settingState = SettingState::ProcessMouse;
        blinker.push(0, 0, 0);
      }
      break;
  }
}

void loop() {
  if (pSerialMouse->requiresInit()) {
    // disable the PS/2 mouse data reporting while the serial port is being initialized
    pPs2Mouse->setReporting(false);
    pSerialMouse->init();
    // Renable PS/2 mouse data reporting
    pPs2Mouse->setReporting(true);
  }

  if (pSerialMouse->isInitialized()) {
    processStateMachine();
  }
}

