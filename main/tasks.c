#include <stdint.h>

#include "esp_log.h"
#include "lwip/sockets.h"
#include "esp_task_wdt.h"

#include "config.h"
#include "tasks.h"
#include "global.h"
#include "udp.h"
#include "wifi.h"
#include "joy_data.h"
#include "axis.h"
#include "button.h"
#include "encoder.h"
#include "message.h"

joystick_info_t g_joystick;

static const char* TAG = "tasks";

static const int STATE_PING_PONG = 0;
static const int STATE_SYNCING = 1;

static char _rx_buffer[1472];
static message_common_ping_t _ping_message;
static message_joystick_sync_t _joy_sync_message;
static int _state;
// The last encoder counter.
static int64_t last_counter[NAGI_MAX_NUM_OF_ENCODERS];

/// @brief Update the joystick state.
/// @return True if the joystick state must be updated.
static bool update_joystick_state(void) {
  // Get all data from peripherals.
  read_axis();
  read_button();
  read_encoder();

  // Update rate is 1kHz.
  static TickType_t last_update_time = 0;
  if (xTaskGetTickCount() - last_update_time > pdMS_TO_TICKS(10)) {
    bool is_anything_changed = false;
    for (int i = 0; i < NAGI_MAX_NUM_OF_BUTTONS; ++i) {
      int index = i / 32;
      int bit = i % 32;
      uint32_t old_button_mask = g_joystick.buttons[index] & (1 << bit);
      uint8_t state = g_button_data[i].stable_state;
      uint32_t new_button_mask = state ? (1 << bit) : 0;
      is_anything_changed |= old_button_mask != new_button_mask;
      g_joystick.buttons[index] = (g_joystick.buttons[index] & ~(1 << bit)) | (state << bit);
    }
    int32_t* axes = &g_joystick.axis_x;
    for (int i = 0; i < NAGI_MAX_NUM_OF_AXES; ++i) {
      is_anything_changed |= g_axes_data[i] != axes[i];
      axes[i] = g_axes_data[i];
    }
    for (int i = 0; i < NAGI_MAX_NUM_OF_ENCODERS; ++i) {
      int inc_button_id = NAGI_MAX_NUM_OF_BUTTONS + i * 2 + 0;
      int inc_index = (inc_button_id) / 32;
      int inc_bit = (inc_button_id) % 32;
      int dec_button_id = NAGI_MAX_NUM_OF_BUTTONS + i * 2 + 1;
      int dec_index = (dec_button_id) / 32;
      int dec_bit = (dec_button_id) % 32;
      if (g_encoder_data[i].counter > last_counter[i]) { // Clockwise.
        g_joystick.buttons[inc_index] |= (1 << inc_bit);
        g_joystick.buttons[dec_index] &= ~(1 << dec_bit);
        is_anything_changed = true;
      } else if (g_encoder_data[i].counter < last_counter[i]) {  // Counter-clockwise.
        g_joystick.buttons[inc_index] &= ~(1 << inc_bit);
        g_joystick.buttons[dec_index] |= (1 << dec_bit);
        is_anything_changed = true;
      } else {
        uint32_t old_inc_button_mask = g_joystick.buttons[inc_index] & (1 << inc_bit);
        uint32_t old_dec_button_mask = g_joystick.buttons[dec_index] & (1 << dec_bit);
        is_anything_changed |= old_inc_button_mask || old_dec_button_mask;
        g_joystick.buttons[inc_index] &= ~(1 << inc_bit);
        g_joystick.buttons[dec_index] &= ~(1 << dec_bit);
      }
      last_counter[i] = g_encoder_data[i].counter;
    }
    last_update_time = xTaskGetTickCount();
    return is_anything_changed;
  }

  return false;
}

/// @brief Send the data to the server.
/// @param data The data.
/// @param length The length of the data.
/// @return The error code.
static int send_data(const void* data, size_t length) {
  return sendto(
    g_sock,
    data,
    length,
    0,
    (struct sockaddr*)&g_server_addr,
    sizeof(g_server_addr)
  );
}

/// @brief Receive the data from the server.
/// @param source_addr The source address.
/// @return The length of the received data.
static int receive_data(struct sockaddr_storage* source_addr) {
  socklen_t socklen = sizeof(struct sockaddr_storage);
  return recvfrom(
    g_sock,
    _rx_buffer,
    sizeof(_rx_buffer) - 1,
    0,
    (struct sockaddr *)source_addr,
    &socklen
  );
}

/// @brief Check the address is from the server.
/// @param source_addr The source address.
/// @return True if the address is from the server.
static bool is_from_server(const struct sockaddr_storage* source_addr) {
  const struct sockaddr_in* addr = (const struct sockaddr_in*)source_addr;
  return addr->sin_family == AF_INET && addr->sin_port == g_server_addr.sin_port && addr->sin_addr.s_addr == g_server_addr.sin_addr.s_addr;
}

/// @brief State machine for ping-pong.
/// @param is_send_success The flag to indicate the send is successful.
void state_ping_pong(bool* is_send_success) {
  if (!(*is_send_success)) {
    // Send the ping message.
    int err = send_data(
      &_ping_message,
      sizeof(message_common_ping_t)
    );
    if (err < 0) {
      ESP_LOGE(TAG, "Error occurred during sending, sleep 100ms. Errno %d", errno);
      vTaskDelay(pdMS_TO_TICKS(100));
    } else {
      // Receive a reply from the server.
      struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
      int len = receive_data(&source_addr);

      // Received something.
      if (len >= 0) {
        if (is_from_server(&source_addr)) {
          const message_common_pong_t* pong = (const message_common_pong_t*)_rx_buffer;
          // Received 'G' << 24 | 'I' << 16 | 'A' << 8 | 'N' from the server.
          if (pong->magic == ('G' << 24 | 'I' << 16 | 'A' << 8 | 'N')) {
            *is_send_success = true;
            _state = STATE_SYNCING;
            ESP_LOGI(TAG, "Received PONG from the server.");
          } else {
            ESP_LOGW(TAG, "Received unknown magic number %08lX from the server.", pong->magic);
          }
        } else {
          ESP_LOGW(TAG, "Received from unknown source.");
        }
      }
    }
  }
}

/// @brief Send the joystick data.
/// @return True if the data is sent successfully.
static bool send_joystick_data(void) {
  bool is_send_success = false;

  memcpy(&_joy_sync_message.data, &g_joystick, sizeof(joystick_info_t));

  int err = send_data(
    &_joy_sync_message,
    sizeof(message_joystick_sync_t)
  );
  if (err < 0) {
    ESP_LOGE(TAG, "Error occurred during sending, sleep 100ms. Errno %d", errno);
    vTaskDelay(pdMS_TO_TICKS(100));
  } else {
    // Receive a reply from the server.
    struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    int len = receive_data(&source_addr);

    // Received something.
    if (len >= 0) {
      if (is_from_server(&source_addr)) {
        const message_joystick_ack_t* ack = (const message_joystick_ack_t*)_rx_buffer;
        // Received 'O' << 8 | 'K' from the server.
        if (ack->payload == 0x4F4B) {
          is_send_success = true;
        } else {
          ESP_LOGW(TAG, "Received NOK from the server.");
        }
      } else {
        ESP_LOGW(TAG, "Received from unknown source.");
      }
    }
  }

  return is_send_success;
}

/// @brief State machine for joystick syncing.
void state_joystick(bool* is_send_success) {
  // Update the joystick state.
  bool is_joystick_changed = update_joystick_state();

  // Send the joystick data.
  if (!(*is_send_success) || is_joystick_changed) {
    *is_send_success = send_joystick_data();

    // for (int i = 0; i < NAGI_MAX_NUM_OF_AXES; i++) {
    //   ESP_LOGI(TAG, "Axis[%d] data: %d", i, g_axes_data[i]);
    // }
    // for (int i = 0; i < NAGI_MAX_NUM_OF_BUTTONS; i++) {
    //   ESP_LOGI(TAG, "Button[%d] data: %d, %d", i, g_button_data[i].stable_state, g_button_data[i].changed);
    // }
    // for (int i = 0; i < NAGI_MAX_NUM_OF_ENCODERS; i++) {
    //   ESP_LOGI(TAG, "Encoder[%d] data: %lld", i, g_encoder_data[i].counter);
    // }
  }
}

/// @brief Main loop task.
/// @param pvParameters Task parameters.
void main_loop_task(void* pvParameters) {
  TickType_t last_wake_time;
  bool is_send_success = false;

  esp_task_wdt_add(NULL);

  // Initialize the joystick.
  memset(&g_joystick, 0, sizeof(joystick_info_t));
  memset(&_ping_message, 0, sizeof(message_common_ping_t));
  _ping_message.header.major_id = MESSAGE_MAJOR_ID_COMMON;
  _ping_message.header.minor_id = MESSAGE_MINOR_ID_COMMON_PING;
  _ping_message.header.length = sizeof(_ping_message.magic);
  // 'N' << 24 | 'A' << 16 | 'G' << 8 | 'I'
  _ping_message.magic = ('N' << 24 | 'A' << 16 | 'G' << 8 | 'I');
  memset(&_joy_sync_message, 0, sizeof(message_joystick_sync_t));
  _joy_sync_message.header.major_id = MESSAGE_MAJOR_ID_JOYSTICK;
  _joy_sync_message.header.minor_id = MESSAGE_MINOR_ID_JOYSTICK_DATA_SYNC;
  _joy_sync_message.header.length = sizeof(_joy_sync_message.data);

  // Set the state to ping-pong.
  _state = STATE_PING_PONG;

  for (;;) {
    bool is_server_setted = g_server_addr.sin_family == AF_INET && g_server_addr.sin_port != 0;

    // Initialise the last_wake_time variable with the current time.
    last_wake_time = xTaskGetTickCount();

    // Wait for the wifi connected.
    EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group, WIFI_CONNECTED_BIT, false, true, pdMS_TO_TICKS(1000));
    if ((bits & WIFI_CONNECTED_BIT) && is_server_setted) {
      switch (_state) {
        case STATE_PING_PONG:
          state_ping_pong(&is_send_success);
          break;
        case STATE_SYNCING:
          state_joystick(&is_send_success);
          break;
        default:
          break;
      }
    }

    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1));

    // Feed the watchdog.
    esp_task_wdt_reset();
  }
}