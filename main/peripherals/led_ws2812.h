#ifndef __LED_WS2812_H__
#define __LED_WS2812_H__

#include <stdint.h>

typedef struct led_strip_t *led_strip_handle_t;

/// @brief Initialize WS2812 peripheral.
/// @return The result of the initialization.
esp_err_t initialize_ws2812(void);

/// @brief Write the RGB value to the LED strip.
/// @param r The red value.
/// @param g The green value.
/// @param b The blue value.
/// @return The result of the write.
esp_err_t write_ws2812(uint8_t r, uint8_t g, uint8_t b);

/// @brief Clear the LED strip.
/// @return The result of the clear.
esp_err_t clear_ws2812(void);

#endif  // __LED_WS2812_H__