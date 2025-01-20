#ifndef __JOY_DATA_H__
#define __JOY_DATA_H__

#include <stdint.h>

/// @brief Joystick information structure.
typedef struct joystick_info {
  int32_t throttle;
  int32_t rudder;
  int32_t aileron;

  int32_t axis_x;
  int32_t axis_y;
  int32_t axis_z;
  int32_t axis_xrot;
  int32_t axis_yrot;
  int32_t axis_zrot;
  int32_t slider;
  int32_t dial;

  int32_t wheel;
  int32_t accelerator;
  int32_t brake;
  int32_t clutch;
  int32_t steering;

  int32_t axis_vx;
  int32_t axis_vy;
  int32_t axis_vz;

  int32_t axis_vbrx;
  int32_t axis_vbry;
  int32_t axis_vbrz;

  uint32_t buttons[4];  // 32 buttons: 0x00000001 means button 1 is pressed, 0x80000000 -> button 32 is pressed

  uint32_t hats[4];     // Lower 4 bits: HAT switch or 16-bit of continuous HAT switch
} joystick_info_t, *joystick_info_ptr_t;

#endif  // __JOY_DATA_H__