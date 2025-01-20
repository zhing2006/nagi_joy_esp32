#ifndef __ENCODER_H__
#define __ENCODER_H__

// @brief The encoder data.
typedef struct {
  // The left GPIO number.
  gpio_num_t left_gpio_num;
  // The right GPIO number.
  gpio_num_t right_gpio_num;
  // The left last state.
  uint8_t left_last_state;
  // The right last state.
  uint8_t right_last_state;
  // The counter.
  int64_t counter;
} encoder_t;

// @brief The encoder data.
extern encoder_t g_encoder_data[NAGI_MAX_NUM_OF_ENCODERS];

// @brief Initialize the encoder module.
void initialize_encoder(void);

// @brief Read the encoder data.
void read_encoder(void);

#endif // __ENCODER_H__