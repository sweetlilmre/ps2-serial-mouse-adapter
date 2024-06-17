#include "ProMini.h"
#include "Ps2Mouse.h"

static const int RS232_RTS = 3;

static Ps2Mouse mouse;
static bool twoButtons = false;
static bool wheelMouse = false;
static volatile bool doSerialInit = false;
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
      mb = data.middleButton ? 0x10 : 0;
      mb |= (data.wheelMovement & 0x0F);
    } else {
      mb = data.middleButton ? 0x20 : 0;
    }
    sendSerialByte(mb);
  }
}

static void initSerialPort() {

  Serial.println("Starting serial port");
  RS_SETTXHIGH;
  delayMicroseconds(10000);
  sendSerialByte('M');
  if(!twoButtons) {
    // if wheelMouse mode has been set by jumper, confirm that the mouse has the capability
    if(wheelMouse && (mouse.getType() == Ps2Mouse::MouseType::wheelMouse)) {
      // set wheelMouse mode
      sendSerialByte('Z');
    } else {
      // set 3-button (Logitech) mode
      sendSerialByte('3');
    }
    Serial.println(wheelMouse ? "Init Wheel mode" : "Init 3-button mode");
  } else {
    Serial.println("Init 2-button mode");
  }
  delayMicroseconds(10000);
  // Renable PS/2 mouse data reporting
  mouse.setReporting(true);
  // Signal the main loop that the serial port is ready
  serialInitDone = true;
}

static void initPs2Port() {

  Serial.println("Reseting PS/2 mouse");
  
  if (mouse.reset()) {
    Serial.println("PS/2 mouse reset OK");
  } else {
    Serial.println("Failed to reset PS/2 mouse");
  }

  if (mouse.setSampleRate(20)) {
    Serial.println("Sample rate set to 20");
  } else {
    Serial.println("Failed to set sample rate");
  }

  Ps2Mouse::Settings settings;
  if (mouse.getSettings(settings)) {
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
  // disable the PS/2 mouse data reporting while the serial port is being initialized
  mouse.setReporting(false);
  // signal the main loop to initialize the serial port
  doSerialInit = true;
}

void setup() {
  // PS/2 Data input must be initialized shortly after power on,
  // or the mouse will not initialize
  PS2_DIRDATAIN_UP;
  RS_DIRTXOUT;
  JP12_DIRIN_UP;
  JP34_DIRIN_UP;
  LED_DIROUT;
  LED_SET(HIGH);

  Serial.begin(115200);
  twoButtons = (JP12_READ == LOW);
  wheelMouse = (JP34_READ == LOW);

  // PS/2 initialization can take > 400ms for reset to complete.
  // Init the mouse here, determine it's characteristics and then
  // wait for the RTS signal to initialize the serial port comms
  initPs2Port();
  Serial.println("Listening on RTS");
  attachInterrupt(digitalPinToInterrupt(RS232_RTS), resetSerialSignal, FALLING);

  Serial.println("Setup done!");
  LED_SETLOW;
}


void loop() {
  if (doSerialInit) {
    initSerialPort();
    doSerialInit = false;
  }
  Ps2Mouse::Data data;
  if (serialInitDone && mouse.readData(data)) {
    sendToSerial(data);
  }
}
