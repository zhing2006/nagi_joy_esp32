#include <stdint.h>

#include "soc/gpio_num.h"

#include "config.h"
#include "button.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

button_t g_button_data[NAGI_MAX_NUM_OF_BUTTONS];

// @brief Initialize the button module.
void initialize_button(void) {
  g_button_data[0] = (button_t){5, 0, 0, 0, 0, 0, 0};
  g_button_data[1] = (button_t){4, 0, 0, 0, 0, 0, 0};
  g_button_data[2] = (button_t){14, 0, 0, 0, 0, 0, 0};
  g_button_data[3] = (button_t){15, 0, 0, 0, 0, 0, 0};
  g_button_data[4] = (button_t){18, 0, 0, 0, 0, 0, 0};
  g_button_data[5] = (button_t){9, 0, 0, 0, 0, 0, 0};
  g_button_data[6] = (button_t){22, 0, 0, 0, 0, 0, 0};
  g_button_data[7] = (button_t){21, 0, 0, 0, 0, 0, 0};
  g_button_data[8] = (button_t){23, 0, 0, 0, 0, 0, 0};

  gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_DISABLE, // Disable interrupt.
    .mode = GPIO_MODE_INPUT,        // Set as input mode.
    .pin_bit_mask = 0,
    .pull_down_en = 0,              // Disable the internal pull-down resistor.
    .pull_up_en = 1,                // Use the internal pull-up resistor.
  };
  for (int i = 0; i < NAGI_MAX_NUM_OF_BUTTONS; i++) {
    io_conf.pin_bit_mask |= (1ULL << g_button_data[i].gpio_num);
  }
  gpio_config(&io_conf);
}

// @brief Read the button data.
void read_button(void) {
  for (int i = 0; i < NAGI_MAX_NUM_OF_BUTTONS; i++) {
    g_button_data[i].last_state = g_button_data[i].current_state;
    g_button_data[i].current_state = gpio_get_level(g_button_data[i].gpio_num);
    g_button_data[i].current_tick = xTaskGetTickCount();
    if (g_button_data[i].current_state != g_button_data[i].last_state) {
      g_button_data[i].last_tick = g_button_data[i].current_tick;
    }
    uint32_t diff = g_button_data[i].current_tick - g_button_data[i].last_tick;
    if (diff > pdMS_TO_TICKS(NAGI_BUTTON_JITTER_THRESHOLD)) {
      if (g_button_data[i].current_state != g_button_data[i].stable_state) {
        g_button_data[i].changed = 1;
      } else {
        g_button_data[i].changed = 0;
      }
      g_button_data[i].stable_state = g_button_data[i].current_state;
    }
  }
}