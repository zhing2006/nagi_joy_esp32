#include <stdio.h>
#include <stdint.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "esp_console.h"
#include "lwip/sockets.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "soc/gpio_num.h"

#include "config.h"
#include "global.h"
#include "commands.h"
#include "led_ws2812.h"
#include "wifi.h"
#include "udp.h"
#include "axis.h"
#include "button.h"
#include "encoder.h"
#include "tasks.h"

static const char *TAG = "main";

/// @brief Initialize the NVS.
static void initialize_nvs(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}

/// @brief Initialize the filesystem.
static esp_err_t initialize_filesystem(void) {
#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

  static wl_handle_t wl_handle;
  const esp_vfs_fat_mount_config_t mount_config = {
    .max_files = 6, // history.txt, wifi0.txt, wifi1.txt, server.txt, joy.txt
    .format_if_mount_failed = true
  };
  esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_PATH, "storage", &mount_config, &wl_handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to mount FATFS (%s).", esp_err_to_name(err));
  }

  return err;

#undef MOUNT_PATH
#undef HISTORY_PATH
}

/// @brief Main function.
void app_main(void) {
  // Initialize the WS2812 peripheral.
  ESP_ERROR_CHECK(initialize_ws2812());
  write_ws2812(10, 1, 1);

  // Sleep 1.0s for waiting all power stable.
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Initialize the axis module.
  initialize_axis();
  start_axis();

  // Initialize the button module.
  initialize_button();

  // Initialize the encoder module.
  initialize_encoder();

  // Initialize the NVS.
  initialize_nvs();

  esp_console_repl_t *repl = NULL;
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  repl_config.prompt = NAGI_PROMPT_STR ">";
  repl_config.max_cmdline_length = NAGI_MAX_COMMAND_LINE_LENGTH;

  // Initialize the filesystem.
  if (initialize_filesystem() == ESP_OK) {
    repl_config.history_save_path = "/data/history.txt";
    ESP_LOGI(TAG, "Command history enabled.");
  } else {
    ESP_LOGW(TAG, "Command history disabled.");
  }

  // Read host:port from the "/data/server.txt" file.
  FILE *f = fopen("/data/server.txt", "r");
  if (f != NULL) {
    char host[64];
    int port;
    if (fscanf(f, "%63[^:]:%d", host, &port) == 2) {
      ESP_LOGI(TAG, "Server address: %s:%d", host, port);
      g_server_addr.sin_family = AF_INET;
      g_server_addr.sin_port = htons(port);
      inet_pton(AF_INET, host, &g_server_addr.sin_addr);
    }
    fclose(f);
  } else {
    memset(&g_server_addr, 0, sizeof(g_server_addr));
  }

  // Initialize wifi.
  initialize_wifi();

  // Wait wifi started for 10s.
  EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group, WIFI_STARTED_BIT, false, true, pdMS_TO_TICKS(10000));
  if (!(bits & WIFI_STARTED_BIT)) {
    ESP_LOGE(TAG, "Failed to start the wifi after 10s.");
    return;
  }

  // Initialize the UDP client.
  initialize_udp_client();

  // Set the LED to yellow for ready.
  write_ws2812(25, 25, 0);

  // Try auto-connecting to the wifi.
  auto_connect_wifi();

  // Register commands.
  ESP_ERROR_CHECK(esp_console_register_help_command());
  ESP_ERROR_CHECK(register_user_commands());

  // Start the REPL.
  esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
  ESP_ERROR_CHECK(esp_console_start_repl(repl));

  // Start the main loop task.
  xTaskCreatePinnedToCore(
    main_loop_task,
    "main_loop",
    4096,
    NULL,
    5,
    NULL,
    tskNO_AFFINITY
  );
}