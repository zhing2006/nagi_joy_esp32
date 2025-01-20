#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include "joy_data.h"

#define MESSAGE_MAJOR_ID_COMMON 0x0000
#define MESSAGE_MINOR_ID_COMMON_PING 0x0000
#define MESSAGE_MINOR_ID_COMMON_PONG 0x0001

#define MESSAGE_MAJOR_ID_JOYSTICK 0x0001
#define MESSAGE_MINOR_ID_JOYSTICK_DATA_SYNC 0x0000
#define MESSAGE_MINOR_ID_JOYSTICK_DATA_ACK 0x0001

/// @brief The message header.
// Align the struct to 2 bytes.
typedef struct {
  uint16_t major_id;
  uint16_t minor_id;
  uint32_t length;
} message_header_t;

/// @brief The common ping message.
typedef struct {
  message_header_t header;
  uint32_t magic;
} message_common_ping_t;

/// @brief The common pong message.
typedef struct {
  message_header_t header;
  uint32_t magic;
} message_common_pong_t;

/// @brief The joystick data sync message.
typedef struct {
  message_header_t header;
  joystick_info_t data;
} message_joystick_sync_t;

/// @brief The joystick data ack message.
typedef struct {
  message_header_t header;
  uint16_t payload;
} message_joystick_ack_t;

#endif // __MESSAGE_H__