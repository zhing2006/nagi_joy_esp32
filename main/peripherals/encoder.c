#include <stdint.h>

#include "soc/gpio_num.h"

#include "config.h"
#include "encoder.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

encoder_t g_encoder_data[NAGI_MAX_NUM_OF_ENCODERS];

// @brief The ISR handler for the encoder.
static void IRAM_ATTR encoder_isr_handler(void* arg) {
  int encoder_num = (int)arg;
  encoder_t* encoder = &g_encoder_data[encoder_num];

  // Read the GPIO state.
  int left_state = gpio_get_level(encoder->left_gpio_num);
  int right_state = gpio_get_level(encoder->right_gpio_num);

  // Update the counter.
  int current = (left_state << 1) | right_state;
  int last = (encoder->left_last_state << 1) | encoder->right_last_state;
  int transition = (last << 2) | current;
  switch (transition) {
    case 0b0001: // 00 -> 01
    case 0b0111: // 01 -> 11
    case 0b1110: // 11 -> 10
    case 0b1000: // 10 -> 00
      encoder->counter++;
      break;
    case 0b0010: // 00 -> 10
    case 0b1011: // 10 -> 11
    case 0b1101: // 11 -> 01
    case 0b0100: // 01 -> 00
      encoder->counter--;
      break;
    default:
      break;
  }

  // Update the last state.
  encoder->left_last_state = left_state;
  encoder->right_last_state = right_state;
}

// @brief Initialize the encoder module.
void initialize_encoder(void) {
  g_encoder_data[0] = (encoder_t){7, 6, 0, 0, 0};
  g_encoder_data[1] = (encoder_t){20, 19, 0, 0, 0};

  // Setup the GPIO.
  gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_ANYEDGE, // Interrupt.
    .mode = GPIO_MODE_INPUT,        // Set as input mode.
    .pin_bit_mask = 0,
    .pull_down_en = 0,              // Disable the internal pull-down resistor.
    .pull_up_en = 1,                // Use the internal pull-up resistor.
  };
  for (int i = 0; i < NAGI_MAX_NUM_OF_ENCODERS; i++) {
    io_conf.pin_bit_mask |= (1ULL << g_encoder_data[i].left_gpio_num) | (1ULL << g_encoder_data[i].right_gpio_num);
  }
  gpio_config(&io_conf);

  // Install the ISR service.
  gpio_install_isr_service(0);

  // Hook the ISR handler.
  for (int i = 0; i < NAGI_MAX_NUM_OF_ENCODERS; i++) {
    gpio_isr_handler_add(g_encoder_data[i].left_gpio_num, encoder_isr_handler, (void*)i);
    gpio_isr_handler_add(g_encoder_data[i].right_gpio_num, encoder_isr_handler, (void*)i);
  }
}

// @brief Read the encoder data.
void read_encoder(void) {
  // Do nothing.
}