#include <esp_check.h>
#include <esp_log.h>

#include "led_strip.h"

#include "config.h"
#include "led_ws2812.h"

// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

static const char *TAG = "led_ws2812";

led_strip_handle_t g_led_strip;

/// @brief Initialize WS2812 peripheral.
/// @return The result of the initialization.
esp_err_t initialize_ws2812(void) {
  // LED strip general initialization, according to your led board design
  led_strip_config_t strip_config = {
    .strip_gpio_num = NAGI_WS2812_GPIO_NUM, // The GPIO that connected to the LED strip's data line
    .max_leds = NAGI_WS2812_LED_NUM,      // The number of LEDs in the strip,
    .led_model = LED_MODEL_WS2812,        // LED strip model
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color order of the strip: GRB
    .flags = {
      .invert_out = false, // don't invert the output signal
    }
  };

  // LED strip backend configuration: RMT
  led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
    .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
    .mem_block_symbols = 64,               // the memory size of each RMT channel, in words (4 bytes)
    .flags = {
      .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
    }
  };

  esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &g_led_strip);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create LED strip object with RMT backend");
  } else {
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
  }

  return err;
}

/// @brief Write the RGB value to the LED strip.
/// @param r The red value.
/// @param g The green value.
/// @param b The blue value.
/// @return The result of the write.
esp_err_t write_ws2812(uint8_t r, uint8_t g, uint8_t b) {
  esp_err_t err = led_strip_set_pixel(g_led_strip, 0, r, g, b);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set pixel color");
    return err;
  }

  err = led_strip_refresh(g_led_strip);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to refresh the strip");
  }

  return err;
}

/// @brief Clear the LED strip.
/// @return The result of the clear.
esp_err_t clear_ws2812(void) {
  esp_err_t err = led_strip_clear(g_led_strip);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to clear the strip");
  }

  return err;
}
