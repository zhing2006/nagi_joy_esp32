#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <soc/gpio_num.h>

// @brief The button data.
typedef struct {
  // The GPIO number.
  gpio_num_t gpio_num;
  // The current state.
  uint8_t current_state;
  // The last state.
  uint8_t last_state;
  // The current anti-jitter counter.
  uint32_t current_tick;
  // The last anti-jitter tick
  uint32_t last_tick;
  // The stable state.
  uint8_t stable_state;
  // The changed flag.
  uint8_t changed;
} button_t;

// @brief The button data.
extern button_t g_button_data[NAGI_MAX_NUM_OF_BUTTONS];

// @brief Initialize the button module.
void initialize_button(void);

// @brief Read the button data.
void read_button(void);

#endif // __BUTTON_H__