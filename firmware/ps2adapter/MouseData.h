#pragma once
#include <Arduino.h>

typedef enum {
  threeButton,
  wheelMouse,
} mouseType_t;


#define MB_LEFT_BITPOS 0
#define MB_RIGHT_BITPOS 1
#define MB_MIDDLE_BITPOS 2

#define MB_LEFT_MASK   _BV(MB_LEFT_BITPOS)
#define MB_RIGHT_MASK  _BV(MB_RIGHT_BITPOS)
#define MB_MIDDLE_MASK _BV(MB_MIDDLE_BITPOS)

typedef enum
{
  MB_LEFT     = MB_LEFT_MASK,
  MB_RIGHT    = MB_RIGHT_MASK,
  MB_MIDDLE   = MB_MIDDLE_MASK,
  MB_ALL      = MB_LEFT | MB_RIGHT | MB_MIDDLE,
} mouseButtons_t;


#define MB_L_TO_R(val)  ((val << 1) & MB_RIGHT_MASK)
#define MB_R_TO_L(val)  ((val >> 1) & MB_LEFT_MASK)

#define MB_LEFT_STATE(val)   (val & MB_LEFT_MASK)
#define MB_RIGHT_STATE(val)  ((val & MB_RIGHT_MASK) >> 1)
#define MB_MIDDLE_STATE(val) ((val & MB_MIDDLE_MASK) >> 2)

typedef struct PS2REPORT
{
  uint8_t buttons;
  int8_t  x;
  int8_t  y;
  int8_t  wheel;
} Ps2Report_t;

typedef struct PS2SETTINGS {
  uint8_t buttons;
  bool scaling;
  bool enable;
  bool remoteMode;
  byte resolution;
  byte sampleRate;
} Ps2Settings_t;

